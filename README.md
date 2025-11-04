# Overview

Jpegmt is the JPEG image format encoding library that enables multithreaded execution to speedup compression of every single image. It also uses SIMD instructions to optimize performance (only for x86 for now).
It supports a limited number of image pixel formats for now: 8-bit grayscale and 32 bit(8 bit per channel) RGBA/BGRA. Only RGB to YCC411 color space conversion is now supported.
Jpegmt is written using only C++ language and doesn’t use any external library dependencies except optional libpng and libjpeg/libjpeg-turbo for library test application.
It uses cmake makefile for configuring and building.

# Quick start

Using jpegmt is quite simple (see _test/src/main.cpp_ for usage example). The library provides __Jpeg::Writer__ class (defined in _JpegWriter.h_ header file) and two interface classes: __Jpeg::OutputStream__ (_JpegWriter.h_) and __Jpeg::ThreadPool__ (_JpegThreadPool.h_) for output stream and thread pool abstractions. It also provides __Jpeg::ImageMetaData__ struct (_JpegImageMetaData.h_) for image format and layout description.
You should create the __Jpeg::OutputStream__ implementation first. E.g. for file output stream:

```c++
#include <fstream>
#include <Jpeg/JpegWriter.h>

class JpegFileOutputStream : public Jpeg::OutputStream
{
public:
  JpegFileOutputStream(const std::string& path) : m_path(path)
  {
  };

  bool open()
  {
    return m_file.open(m_path, std::ios::binary | std::ios::out) != nullptr;
  }

  void close()
  {
    m_file.close();
  }

  // override Jpeg::OutputStream virtual method
  int64_t writeJpegBytes(const char* bytes, int64_t count) override
  {
    return m_file.sputn(bytes, count);
  }

protected:
  std::filebuf m_file;
  std::string m_path;
};
```

__Jpeg::ThreadPool__ implementation should be created if multithreaded execution is required. It may be your own implementation or some wrapper for existing thread pool object from your project. Example of _Helper_ submodule __ThreadPool__ wrapper:

```c++
#include <Helper/ThreadPool.h>
#include <Jpeg/JpegThreadPool.h>

// do not create new instance of that class for each image compression
// try to create single instance per app or per thread since the creation is quite expensive
// then use that instance many times
class JpegThreadPool : public Jpeg::ThreadPool
{
public:
  JpegThreadPool(int maxThreadCount = -1) : m_threadPool(maxThreadCount) {};

protected:
  // maximum number of threads for parallel execution
  int getMaxThreadCount() const override
  {
    return m_threadPool.getThreadCount();
  }

  // execute function f() in parallel, processing specified range in each thread
  void executeWorkers(const WorkerFunction& f, const std::vector< std::pair<int64_t, int64_t> >& ranges) override
  {
    for(size_t i = 0; i < ranges.size(); i++)
    {
      m_threadPool.addJob([i, ranges, f]()
        {
          f((int)i, ranges.at(i).first, ranges.at(i).second);
        });
    }
    m_threadPool.waitJobs();
  }

protected:
  Helper::ThreadPool m_threadPool;
};
```

Instances of those classes should be created and pointers to them passed to __Jpeg::Writer__ constructor (__nullptr__ for __Jpeg::ThreadPool__ may be provided for single thread execution):

```c++
JpegFileOutputStream f("test.jpg");
if (!f.open())
  exit(1);

std::unique_ptr<JpegThreadPool> threadPool;
// create if multithreaded execution required
threadPool = std::make_unique<JpegThreadPool>(threadCount);

Jpeg::Writer jpegWriter(&f, threadPool.get());
```

Then fill __Jpeg::ImageMetaData__ struct fields with corresponding image layout and format data and pass it to __Jpeg::Writer::write()__ along with a pointer to pixel data to compress the image and write result to output stream:

```c++
#include <Jpeg/JpegImageMetaData.h>

int bytesPerPixel = …
Jpeg::ImageMetaData imageMetaData;
imageMetaData.m_size = Jpeg::Size{width, height};
imageMetaData.m_scanlineBytes = width * bytesPerPixel; // may be aligned for better SIMD performance
imageMetaData.m_format = …
uint8_t* pixels = …

jpegWriter.write(imageMetaData, pixels);
```


