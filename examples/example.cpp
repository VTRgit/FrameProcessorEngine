#include <fpe/frame_processor_engine.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <tuple>

template <std::size_t WIDTH, std::size_t HEIGHT> class Image {
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
  using ImageRingBuffer128 = fpe::RingBuffer<Image<128, 128>, 5>;
  using ImageRingBuffer256 = fpe::RingBuffer<Image<256, 256>, 4>;
  using ImageRingBuffer320 = fpe::RingBuffer<Image<320, 320>, 3>;

  ImageRingBuffer128 ringBuff128;
  ImageRingBuffer256 ringBuff256;
  ImageRingBuffer320 ringBuff320;

  std::tuple<ImageRingBuffer128, ImageRingBuffer256, ImageRingBuffer320>
      ringBuffers(std::move(ringBuff128), std::move(ringBuff256),
                  std::move(ringBuff320));

  fpe::FrameProcessorEngine frameProcessorEngine(ringBuffers);
  frameProcessorEngine.start();
  frameProcessorEngine.stop();

  ringBuff128.size();

  return 0; // Return 0 to indicate successful execution
}