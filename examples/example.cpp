#include <fpe/frame_processor_engine.hpp>

#include <array>
#include <cstdint>
#include <iostream>

constexpr int WIDTH = 128;
constexpr int HEIGHT = 128;

class Image {
public:
  // Pixel representation using 4 channels (R, G, B, A)
  struct Pixel {
    uint8_t r, g, b, a;

    // Default to opaque black
    Pixel(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0,
          uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
  };

  // Statically allocated array for pixels
  using ImageData = std::array<Pixel, WIDTH * HEIGHT>;

  Image() : data{} {}

  // Set a pixel's color at (x, y) with boundary checking
  void setPixel(int x, int y, const Pixel &pixel) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
      data[y * WIDTH + x] = pixel;
    }
  }

  // Get a pixel's color at (x, y) with boundary checking
  Pixel getPixel(int x, int y) const {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
      return data[y * WIDTH + x];
    }
    return Pixel(); // Return default pixel if out of bounds
  }

  // Print image information (for debugging purposes)
  void printInfo() const {
    std::cout << "Image Dimensions: " << WIDTH << "x" << HEIGHT << '\n';
    std::cout << "Total Pixels: " << data.size() << '\n';
  }

private:
  ImageData data; // Statically allocated pixel data
};

int main() {
  // fpe::RingBuffer<Image> ringBuffers[5];

  const std::array<fpe::RingBuffer<Image, 10>, 3> ringBuffers;
  std::span<const fpe::RingBuffer<Image, 10>> bufferSpan(ringBuffers);

  fpe::FrameProcessorEngine frameProcessorEngine(bufferSpan);
  frameProcessorEngine.start();
  frameProcessorEngine.stop();

  return 0; // Return 0 to indicate successful execution
}