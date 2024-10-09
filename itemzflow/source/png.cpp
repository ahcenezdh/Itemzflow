#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <GLES2/gl2.h>
#ifdef __ORBIS__
#include <user_mem.h> 
#endif
#include "defines.h"
#include <png.h>
#include <string>
#include <unordered_map>
#include <future>
#include <memory>
#include <atomic>
#include <mutex>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class TextureManager {
private:
    std::unordered_map<std::string, GLuint> textureCache;
    std::mutex cacheMutex;

    GLuint loadTextureInternal(const GLsizei width, const GLsizei height, const GLenum type, const GLvoid *pixels) {
        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
        return textureId;
    }

public:
    GLuint loadTexture(const std::string& path) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = textureCache.find(path);
        if (it != textureCache.end()) {
            return it->second;
        }

        int width, height, channels;
        unsigned char* image = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!image) {
            log_error("Failed to load texture: %s", path.c_str());
            return 0;
        }

        GLuint textureId = loadTextureInternal(width, height, GL_RGBA, image);
        stbi_image_free(image);

        textureCache[path] = textureId;
        return textureId;
    }

    void clearCache() {
        std::lock_guard<std::mutex> lock(cacheMutex);
        for (const auto& pair : textureCache) {
            glDeleteTextures(1, &pair.second);
        }
        textureCache.clear();
    }

    ~TextureManager() {
        clearCache();
    }
};

TextureManager g_textureManager;

GLuint load_png_asset_into_texture(const char* relative_path) {
    return g_textureManager.loadTexture(relative_path);
}

struct PngWriteData {
    int width;
    int height;
    std::unique_ptr<int[]> buffer;
    std::string filename;
    std::string title;
};

void writeImageAsync(PngWriteData&& data) {
    FILE *fp = fopen(data.filename.c_str(), "wb");
    if (!fp) {
        log_error("Could not open file %s for writing", data.filename.c_str());
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        log_error("Could not allocate write struct");
        fclose(fp);
        return;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        log_error("Could not allocate info struct");
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        log_error("Error during png creation");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, data.width, data.height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    if (!data.title.empty()) {
        png_text title_text;
        title_text.compression = PNG_TEXT_COMPRESSION_NONE;
        title_text.key = const_cast<char*>("Title");
        title_text.text = const_cast<char*>(data.title.c_str());
        png_set_text(png_ptr, info_ptr, &title_text, 1);
    }

    png_write_info(png_ptr, info_ptr);

    std::vector<png_byte> row(3 * data.width);

    for (int y = data.height - 1; y >= 0; --y) {
        for (int x = 0; x < data.width; ++x) {
            int* pixel = &data.buffer[y * data.width + x];
            row[x * 3] = (*pixel >> 16) & 0xFF;
            row[x * 3 + 1] = (*pixel >> 8) & 0xFF;
            row[x * 3 + 2] = *pixel & 0xFF;
        }
        png_write_row(png_ptr, row.data());
    }

    png_write_end(png_ptr, nullptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

void writeImage(const char* filename, int width, int height, int* buffer, const char* title) {
    PngWriteData data{width, height, std::unique_ptr<int[]>(buffer), filename, title ? title : ""};
    std::thread(writeImageAsync, std::move(data)).detach();
}

bool is_png_valid(const char* relative_path, int* width, int* height) {
    int channels;
    unsigned char* data = stbi_load(relative_path, width, height, &channels, 0);
    if (data) {
        stbi_image_free(data);
        return true;
    }
    return false;
}

void load_png_cover_data_into_texture(struct AppIcon::ImageData& data, std::atomic<bool>& needs_loaded, std::atomic<GLuint>& tex) {
    if (!data.data) {
        log_error("Failed to load texture data");
        return;
    }

    tex = g_textureManager.loadTextureInternal(data.w, data.h, GL_RGBA, data.data.get());
    log_info("Loaded texture: %d", tex.load());

    data.data.reset();
    needs_loaded = false;
}

GLuint load_png_data_into_texture(const char* data, int size) {
    int width, height, channels;
    unsigned char* image = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data), size, &width, &height, &channels, STBI_rgb_alpha);
    if (!image) {
        log_error("Failed to load texture from memory");
        return 0;
    }

    GLuint tex = g_textureManager.loadTextureInternal(width, height, GL_RGBA, image);
    stbi_image_free(image);
    return tex;
}