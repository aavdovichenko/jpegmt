#pragma once

#include <Jpeg/Rgb.h>

#include <Helper/Platform/Cpu/simd.h>

namespace Jpeg
{

struct Rgb8ToYccTable : public Rgb8ToYcc
{
  struct Weights
  {
    FixedPoint::Type m_rWeights[256];
    FixedPoint::Type m_gWeights[256];
    FixedPoint::Type m_bWeights[256];

    inline int32_t rgbToComponentValue(int r, int g, int b) const;
    template<ImageMetaData::Format rgbFormat> inline int32_t rgbToComponentValue(uint32_t rgb) const;
  };

  Weights m_yTable;
  Weights m_cbTable;
  Weights m_crTable;

  template<ImageMetaData::Format format> inline int32_t rgbToY(uint32_t rgb) const;
  template<ImageMetaData::Format format> inline int32_t rgbToCb(uint32_t rgb) const;
  template<ImageMetaData::Format format> inline int32_t rgbToCr(uint32_t rgb) const;
};

template <typename T, int SimdLength, int cbcrAddFractionBits> struct RgbToYcc;

template <int SimdLength, int cbcrAddFractionBits>
struct RgbToYcc<int16_t, SimdLength, cbcrAddFractionBits> : public Rgb8ToYcc
{
  typedef Platform::Cpu::SIMD<int16_t, SimdLength> SimdHelper;
  typedef typename SimdHelper::Type SimdType;
  typedef typename Platform::Cpu::SIMD<int16_t, SimdLength>::ExtendedType ExtendedSimdType;

  static SimdType y(SimdType r, SimdType g, SimdType b)
  {
    constexpr int32_t ybgWeight = FixedPoint::fromDouble(0.25000);
    constexpr int32_t yrgWeight = ygWeight - ybgWeight;

    return (SimdHelper::template mulAdd<yrWeight, yrgWeight>(r, g) + SimdHelper::template mulAdd<ybWeight, ybgWeight>(b, g) + ExtendedSimdType::populate(yOffset)).template descale<FixedPoint::fractionBits>();
  }

  static SimdType cb(SimdType r, SimdType g, SimdType b, int32_t bias = 0)
  {
    int32_t offset = cbcrOffset<cbcrAddFractionBits>();
    if (cbcrAddFractionBits)
      offset += bias;
    // TODO: unsigned extend()
    return (SimdHelper::template mulAdd<cbrWeight, cbgWeight>(r, g) + (SimdHelper::extend(b) << (FixedPoint::fractionBits - 1)) + ExtendedSimdType::populate(offset)).template descale<FixedPoint::fractionBits + cbcrAddFractionBits>();
  }

  static SimdType cr(SimdType r, SimdType g, SimdType b, int32_t bias = 0)
  {
    int32_t offset = cbcrOffset<cbcrAddFractionBits>();
    if (cbcrAddFractionBits)
      offset += bias;
    // TODO: unsigned extend()
    return (SimdHelper::template mulAdd<crgWeight, crbWeight>(g, b) + (SimdHelper::extend(r) << (FixedPoint::fractionBits - 1)) + ExtendedSimdType::populate(offset)).template descale<FixedPoint::fractionBits + cbcrAddFractionBits>();
  }
};

template <int SimdLength, int cbcrAddFractionBits>
struct RgbToYcc<int32_t, SimdLength, cbcrAddFractionBits> : public Rgb8ToYcc
{
  typedef Platform::Cpu::SIMD<int32_t, SimdLength> SimdHelper;
  typedef typename SimdHelper::Type SimdType;

  static SimdType y(SimdType r, SimdType g, SimdType b)
  {
    return (r * yrWeight + g * ygWeight + b * ybWeight + SimdHelper::populate(yOffset)) >> FixedPoint::fractionBits;
  }

  static SimdType cb(SimdType r, SimdType g, SimdType b, int32_t bias = 0)
  {
    return ((b << (FixedPoint::fractionBits - 1)) + r * cbrWeight + g * cbgWeight + SimdHelper::populate(cbcrOffset<cbcrAddFractionBits>() + bias)) >> (FixedPoint::fractionBits + cbcrAddFractionBits);
  }

  static SimdType cr(SimdType r, SimdType g, SimdType b, int32_t bias = 0)
  {
    return ((r << (FixedPoint::fractionBits - 1)) + g * crgWeight + b * crbWeight + SimdHelper::populate(cbcrOffset<cbcrAddFractionBits>() + bias)) >> (FixedPoint::fractionBits + cbcrAddFractionBits);
  }
};

template <typename T, int cbcrAddFractionBits>
struct RgbToYccNoSimd : public Rgb8ToYcc
{
  static T y(T r, T g, T b)
  {
    return (r * yrWeight + g * ygWeight + b * ybWeight + yOffset) >> FixedPoint::fractionBits;
  }

  static T cb(T r, T g, T b, int32_t bias = 0)
  {
    return ((b << (FixedPoint::fractionBits - 1)) + r * cbrWeight + g * cbgWeight + cbcrOffset<cbcrAddFractionBits>() + bias) >> (FixedPoint::fractionBits + cbcrAddFractionBits);
  }

  static T cr(T r, T g, T b, int32_t bias = 0)
  {
    return ((r << (FixedPoint::fractionBits - 1)) + g * crgWeight + b * crbWeight + cbcrOffset<cbcrAddFractionBits>() + bias) >> (FixedPoint::fractionBits + cbcrAddFractionBits);
  }
};

template <int cbcrAddFractionBits>
struct RgbToYcc<int16_t, 1, cbcrAddFractionBits> : public RgbToYccNoSimd<int16_t, cbcrAddFractionBits>
{
};

template <int cbcrAddFractionBits>
struct RgbToYcc<int32_t, 1, cbcrAddFractionBits> : public RgbToYccNoSimd<int32_t, cbcrAddFractionBits>
{
};

// implementation

inline int32_t Jpeg::Rgb8ToYccTable::Weights::rgbToComponentValue(int r, int g, int b) const
{
  return FixedPoint::toInt32(m_rWeights[r] + m_gWeights[g] + m_bWeights[b]);
}

template<ImageMetaData::Format rgbFormat>
inline int32_t Rgb8ToYccTable::Weights::rgbToComponentValue(uint32_t rgb) const
{
  return rgbToComponentValue(Rgb<rgbFormat>::red(rgb), Rgb<rgbFormat>::green(rgb), Rgb<rgbFormat>::blue(rgb));
}

template<ImageMetaData::Format format>
inline int32_t Rgb8ToYccTable::rgbToY(uint32_t rgb) const
{
  return m_yTable.rgbToComponentValue<format>(rgb);
}

template<ImageMetaData::Format format>
inline int32_t Rgb8ToYccTable::rgbToCb(uint32_t rgb) const
{
  return m_cbTable.rgbToComponentValue<format>(rgb);
}

template<ImageMetaData::Format format>
inline int32_t Rgb8ToYccTable::rgbToCr(uint32_t rgb) const
{
  return m_crTable.rgbToComponentValue<format>(rgb);
}

}
