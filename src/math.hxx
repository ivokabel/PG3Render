#pragma once 

#include "debugging.hxx"
#include "types.hxx"

#include <cmath>
#include <algorithm>

namespace Math
{
    // Due to portability issues
    const float kPiF        = 3.14159265358979f;
    const float kPiInvF     = 1.f / Math::kPiF;
    const float kPiDiv2F    = 1.5707963f;

    float  InfinityF() { return std::numeric_limits<float>::infinity(); }
    double InfinityD() { return std::numeric_limits<double>::infinity(); }

    // Tools be used instead of floating point values comparison against zero.
    // For IEEE floats and doubles this constant was set to 2^-20 and 2^-49 respectively, resulting in
    // 8 * float_eps and 8 * double_eps respectively.
    // Originally, this was set to  2^-21 and 2^-50 respectively, resulting in 4 * float_eps and 
    // 4 * double_eps respectively, but it was too sensitive in some situations.

    //const float kTinyF    = 4.76837158203125E-7f;             // 2^-21
    //const double kTinyD   = 8.8817841970012523233891E-16;     // 2^-50
    const float kTinyF      = 9.5367431640625E-7f;              // 2^-20
    const double kTinyD     = 1.7763568394002504646778E-15;     // 2^-49

    inline float IsTiny(float x)
    {
        return std::abs(x) < Math::kTinyF;
    }

    inline double IsTiny(double x)
    {
        return std::abs(x) < Math::kTinyD;
    }

    // Defines huge float and double constants that can be regarded infinity for
    // this type of floating point arithmetic.
    //
    // For IEEE floats this constant was set to 2^31, so that multiplying
    // its exponent by 4 yields a float that is still valid.
    //
    // For  IEEE doubles this constant was set to 2^255, so that multiplying
    // its exponent by 4 yields a double that is still valid.

    const float kHugeF  = 2.147483648E+9f;
    const double kHugeD = 5.78960446186580977117855E+76;

    //////////////////////////////////////////////////////////////////////////
    // Math section

    float DegToRad(float deg)
    {
        return (deg / 360.f) * (2.f * Math::kPiF);
    }

    float RadToDeg(float rad)
    {
        return 360.f * (rad / (2.f * Math::kPiF));
    }

    template<typename T>
    T Sqr(const T& a)
    {
        static_assert(std::is_arithmetic<T>::value, "T must be arithmetic");

        return a*a;
    }

    template<typename T>
    T SafeSqrt(const T& a)
    {
        return std::sqrt(std::max<T>(0, a));
    }

    template<typename T>
    T SignNum(const T& a)
    {
        return std::signbit(a) ? T(-1) : T(1);
    }

    float FmodX(float x, float y)
    {
        float result = fmod(x, y);
        if (result < 0.0f)
            result += y;

        PG3_ASSERT_FLOAT_IN_RANGE(result, 0.0f, y);

        return result;
    }

    template <typename T>
    inline T Clamp(const T& n, const T& lower, const T& upper)
    {
        PG3_ASSERT(lower <= upper);
        return std::max(lower, std::min(n, upper));
    }

    template <typename T>
    inline T Min3(const T& n1, const T& n2, const T& n3)
    {
        return std::min(n1, std::min(n2, n3));
    }

    template <typename T>
    inline T Max3(const T& n1, const T& n2, const T& n3)
    {
        return std::max(n1, std::max(n2, n3));
    }

    template <
        typename T,
        typename = typename
            std::enable_if<
                   std::is_same<double, T>::value
                || std::is_same<float, T>::value
                >::type
        >
    T Lerp(T c, T x1, T x2)
    {
        return ((T)1. - c) * x1 + c * x2;
    }

    // 50-year old atan approximation due to Hastings
    // Taken from http://lists.apple.com/archives/PerfOptimization-dev/2005/Jan/msg00051.html
    // |error| < 0.005
    PG3_PROFILING_NOINLINE
    float FastAtan2(float y, float x)
    {
        if (x == 0.0f)
        {
            if (y > 0.0f)
                return Math::kPiDiv2F;
            if (y == 0.0f)
                return 0.0f;
            return -Math::kPiDiv2F;
        }

        float atan;
        const float z = y / x;
        if (fabsf(z) < 1.0f)
        {
            atan = z / (1.0f + 0.28f*z*z);
            if (x < 0.0f)
            {
                if (y < 0.0f)
                    return atan - Math::kPiF;
                return atan + Math::kPiF;
            }
        }
        else
        {
            atan = Math::kPiDiv2F - z / (z*z + 0.28f);
            if (y < 0.0f)
                return atan - Math::kPiF;
        }

        return atan;
    }
}

template<typename T>
class Vec2Base
{
    static_assert(std::is_arithmetic<T>::value, "T must be arithmetic");

public:

    Vec2Base() {}
    Vec2Base(T a) : x(a),y(a) {}
    Vec2Base(T a, T b) : x(a),y(b) {}

    const T& Get(uint32_t a) const { return reinterpret_cast<const T*>(this)[a]; }
    T&       Get(uint32_t a)       { return reinterpret_cast<T*>(this)[a]; }

    // Unary minus
    Vec2Base<T> operator-() const
    { Vec2Base<T> res; res.x = -x; res.y = -y; return res; }

    // Binary operations
    friend Vec2Base<T> operator+(const Vec2Base& a, const Vec2Base& b)
    { Vec2Base<T> res; res.x = a.x + b.x; res.y = a.y + b.y; return res; }
    friend Vec2Base<T> operator-(const Vec2Base& a, const Vec2Base& b)
    { Vec2Base<T> res; res.x = a.x - b.x; res.y = a.y - b.y; return res; }
    friend Vec2Base<T> operator*(const Vec2Base& a, const Vec2Base& b)
    { Vec2Base<T> res; res.x = a.x * b.x; res.y = a.y * b.y; return res; }
    friend Vec2Base<T> operator/(const Vec2Base& a, const Vec2Base& b)
    { Vec2Base<T> res; res.x = a.x / b.x; res.y = a.y / b.y; return res; }

    Vec2Base<T>& operator+=(const Vec2Base& a)
    { x += a.x; y += a.y; return *this; }
    Vec2Base<T>& operator-=(const Vec2Base& a)
    { x -= a.x; y -= a.y; return *this; }
    Vec2Base<T>& operator*=(const Vec2Base& a)
    { x *= a.x; y *= a.y; return *this; }
    Vec2Base<T>& operator/=(const Vec2Base& a)
    { x /= a.x; y /= a.y; return *this; }

    friend T Dot(const Vec2Base& a, const Vec2Base& b)
    { return a.x * b.x + a.y * b.y; }

    //friend Vec2Base Clamp(const Vec2Base& val, const Vec2Base& lower, const Vec2Base& upper)
    //{
    //    PG3_ASSERT(lower.x <= upper.x);
    //    PG3_ASSERT(lower.y <= upper.y);
    //    Vec2Base<T> res;
    //    res.x = std::max(lower.x, std::min(val.x, upper.x));
    //    res.y = std::max(lower.y, std::min(val.y, upper.y));
    //    return res;
    //}

public:

    T x, y;
};

typedef Vec2Base<float>     Vec2f;
typedef Vec2Base<int32_t>   Vec2i;
typedef Vec2Base<uint32_t>  Vec2ui;

template<typename T>
class Vec3Base
{
    static_assert(std::is_arithmetic<T>::value, "T must be arithmetic");

public:

    Vec3Base() {}
    Vec3Base(T a) : x(a),y(a),z(a) {}
    Vec3Base(T a, T b, T c) : x(a),y(b),z(c) {}

    // For the double specialization of this template, we add the conversion constructor 
    // from the float specialization. This is one way of allowing binary operators to accept
    // one float and one double parameter while ensuring the whole operation is executed 
    // in double precision. This is, for instance, used during accumulation of float radiance 
    // values in the double framebuffer.
    template <
        typename T2,
        typename = typename
            std::enable_if<
                   std::is_same<double, T>::value
                && std::is_same<float, T2>::value
                >::type
        >
    Vec3Base(const Vec3Base<T2> &a) :
        x(static_cast<double>(a.x)),
        y(static_cast<double>(a.y)),
        z(static_cast<double>(a.z)) {}

    void        Set(T aX, T aY, T aZ) { x = aX; y = aY; z = aZ; }

    const T&    Get(uint32_t a) const { return reinterpret_cast<const T*>(this)[a]; }
    T&          Get(uint32_t a)       { return reinterpret_cast<T*>(this)[a]; }
    Vec2Base<T> GetXY() const         { return Vec2Base<T>(x, y); }

    T           Max()   const         { return std::max(std::max(x, y), z); }
    bool        IsZero() const        { return (x == 0) && (y == 0) && (z == 0); }

    Vec3Base<T>& ClipProportionally(const float aMaxAllowedVal)
    {
        const float maxVal = Max();
        if (maxVal > aMaxAllowedVal)
        {
            const float scale = aMaxAllowedVal / maxVal;
            x *= scale;
            y *= scale;
            z *= scale;
        }
        return *this;
    }

    // Unary minus
    Vec3Base<T> operator-() const
    { Vec3Base<T> res; res.x = -x; res.y = -y; res.z = -z; return res; }

    // Binary operations
    friend Vec3Base<T> operator+(const Vec3Base& a, const Vec3Base& b)
    { Vec3Base<T> res; res.x = a.x + b.x; res.y = a.y + b.y; res.z = a.z + b.z; return res; }
    friend Vec3Base<T> operator-(const Vec3Base& a, const Vec3Base& b)
    { Vec3Base<T> res; res.x = a.x - b.x; res.y = a.y - b.y; res.z = a.z - b.z; return res; }
    friend Vec3Base<T> operator*(const Vec3Base& a, const Vec3Base& b)
    { Vec3Base<T> res; res.x = a.x * b.x; res.y = a.y * b.y; res.z = a.z * b.z; return res; }
    friend Vec3Base<T> operator/(const Vec3Base& a, const Vec3Base& b)
    { Vec3Base<T> res; res.x = a.x / b.x; res.y = a.y / b.y; res.z = a.z / b.z; return res; }

    Vec3Base<T>& operator+=(const Vec3Base& a)
    { x += a.x; y += a.y; z += a.z; return *this; }
    Vec3Base<T>& operator-=(const Vec3Base& a)
    { x -= a.x; y -= a.y; z -= a.z; return *this; }
    Vec3Base<T>& operator*=(const Vec3Base& a)
    { x *= a.x; y *= a.y; z *= a.z; return *this; }
    Vec3Base<T>& operator/=(const Vec3Base& a)
    { x /= a.x; y /= a.y; z /= a.z; return *this; }

    //PG3_NOINLINE
    friend T Dot(const Vec3Base& a, const Vec3Base& b)
    { return a.x * b.x + a.y * b.y + a.z * b.z; }

    friend Vec3Base Cross(const Vec3Base &a, const Vec3Base &b)
    {
        Vec3Base res;
        res.x = a.y * b.z - a.z * b.y;
        res.y = a.z * b.x - a.x * b.z;
        res.z = a.x * b.y - a.y * b.x;
        return res;
    }

    //PG3_NOINLINE
    friend Vec3Base Normalize(const Vec3Base& a)
    {
        const T lenSqr = Dot(a, a);
        const T len    = std::sqrt(lenSqr);
        return a / len;
    }

    float           LenSqr() const  { return Dot(*this, *this);   }
    float           Length() const  { return std::sqrt(LenSqr()); }
    Vec3Base<T>&    Normalize()     { this->operator/=(Length()); return *this; }

public:

    T x, y, z;
};

typedef Vec3Base<float>     Vec3f;
typedef Vec3Base<double>    Vec3d;
typedef Vec3Base<int32_t>   Vec3i;
typedef Vec3Base<uint32_t>  Vec3ui;

class Mat4f
{
public:

    Mat4f(){}
    Mat4f(float a){ for (uint32_t i=0; i<16; i++) GetPtr()[i] = a; }

    const float* GetPtr() const { return reinterpret_cast<const float*>(this); }
    float*       GetPtr()       { return reinterpret_cast<float*>(this);       }

    const float& Get(uint32_t r, uint32_t c) const { return GetPtr()[r + c*4]; }
    float&       Get(uint32_t r, uint32_t c)       { return GetPtr()[r + c*4]; }

    void SetRow(uint32_t r, float a, float b, float c, float d)
    {
        Get(r, 0) = a;
        Get(r, 1) = b;
        Get(r, 2) = c;
        Get(r, 3) = d;
    }

    void SetRow(uint32_t r, const Vec3f &a, float b)
    {
        for (uint32_t i=0; i<3; i++)
            Get(r, i) = a.Get(i);

        Get(r, 3) = b;
    }

    Vec3f TransformVector(const Vec3f& aVec) const
    {
        Vec3f res(0);
        for (uint32_t r=0; r<3; r++)
            for (uint32_t c=0; c<3; c++)
                res.Get(r) += aVec.Get(c) * Get(r, c);

        return res;
    }

    Vec3f TransformPoint(const Vec3f& aVec) const
    {
        float w = Get(3,3);

        for (uint32_t c=0; c<3; c++)
            w += Get(3, c) * aVec.Get(c);

        const float invW = 1.f / w;

        Vec3f res(0);

        for (uint32_t r=0; r<3; r++)
        {
            res.Get(r) = Get(r, 3);

            for (uint32_t c=0; c<3; c++)
                res.Get(r) += aVec.Get(c) * Get(r, c);

            res.Get(r) *= invW;
        }
        return res;
    }

    static Mat4f MakeZero() { Mat4f res(0); return res; }

    static Mat4f Indetity()
    {
        Mat4f res(0);
        for (uint32_t i=0; i<4; i++) res.Get(i,i) = 1.f;
        return res;
    }

    static Mat4f Scale(const Vec3f& aScale)
    {
        Mat4f res = Mat4f::Indetity();
        for (uint32_t i=0; i<3; i++) res.Get(i,i) = aScale.Get(i);
        res.Get(3,3) = 1;
        return res;
    }

    static Mat4f Translate(const Vec3f& aScale)
    {
        Mat4f res = Mat4f::Indetity();
        for (uint32_t i=0; i<3; i++) res.Get(i,3) = aScale.Get(i);
        res.Get(3,3) = 1;
        return res;
    }

    static Mat4f Perspective(
        float aFovX,
        float aFovY,
        float aNear,
        float aFar)
    {
        PG3_ASSERT_FLOAT_LESS_THAN(0.f, aNear);
        PG3_ASSERT_FLOAT_LESS_THAN(aNear, aFar);

        // Camera points towards -z.  0 < near < far.
        // Matrix maps z range [-near, -far] to [-1, 1], after homogeneous division.

        const float fx = 1.f / std::tan(Math::DegToRad(0.5f * aFovX));
        const float fy = 1.f / std::tan(Math::DegToRad(0.5f * aFovY));
        const float d  = 1.f / (aNear - aFar);

        Mat4f r;
        r.m00 = fx;   r.m01 = 0.0f; r.m02 = 0.0f;               r.m03 = 0.0f;
        r.m10 = 0.0f; r.m11 = -fy;  r.m12 = 0.0f;               r.m13 = 0.0f;
        r.m20 = 0.0f; r.m21 = 0.0f; r.m22 = (aNear + aFar) * d; r.m23 = 2.0f * aNear * aFar * d;
        r.m30 = 0.0f; r.m31 = 0.0f; r.m32 = -1.0f;              r.m33 = 0.0f;

        return r;
    }

    friend Mat4f operator*(const Mat4f& left, const Mat4f& right)
    {
        Mat4f res(0);
        for (uint32_t row=0; row<4; row++)
            for (uint32_t col=0; col<4; col++)
                for (uint32_t i=0; i<4; i++)
                    res.Get(row, col) += left.Get(row, i) * right.Get(i, col);

        return res;
    }

    // Code for inversion taken from:
    // http://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    friend Mat4f Invert(const Mat4f& aMatrix)
    {
        const float *m = aMatrix.GetPtr();
        float inv[16], det;
        uint32_t i;

        inv[0] = m[5] * m[10] * m[15] -
            m[5]  * m[11] * m[14] -
            m[9]  * m[6]  * m[15] +
            m[9]  * m[7]  * m[14] +
            m[13] * m[6]  * m[11] -
            m[13] * m[7]  * m[10];

        inv[4] = -m[4]  * m[10] * m[15] +
            m[4]  * m[11] * m[14] +
            m[8]  * m[6]  * m[15] -
            m[8]  * m[7]  * m[14] -
            m[12] * m[6]  * m[11] +
            m[12] * m[7]  * m[10];

        inv[8] = m[4]  * m[9] * m[15] -
            m[4]  * m[11] * m[13] -
            m[8]  * m[5] * m[15] +
            m[8]  * m[7] * m[13] +
            m[12] * m[5] * m[11] -
            m[12] * m[7] * m[9];

        inv[12] = -m[4]  * m[9] * m[14] +
            m[4]  * m[10] * m[13] +
            m[8]  * m[5] * m[14] -
            m[8]  * m[6] * m[13] -
            m[12] * m[5] * m[10] +
            m[12] * m[6] * m[9];

        inv[1] = -m[1]  * m[10] * m[15] +
            m[1]  * m[11] * m[14] +
            m[9]  * m[2] * m[15] -
            m[9]  * m[3] * m[14] -
            m[13] * m[2] * m[11] +
            m[13] * m[3] * m[10];

        inv[5] = m[0]  * m[10] * m[15] -
            m[0]  * m[11] * m[14] -
            m[8]  * m[2] * m[15] +
            m[8]  * m[3] * m[14] +
            m[12] * m[2] * m[11] -
            m[12] * m[3] * m[10];

        inv[9] = -m[0]  * m[9] * m[15] +
            m[0]  * m[11] * m[13] +
            m[8]  * m[1] * m[15] -
            m[8]  * m[3] * m[13] -
            m[12] * m[1] * m[11] +
            m[12] * m[3] * m[9];

        inv[13] = m[0]  * m[9] * m[14] -
            m[0]  * m[10] * m[13] -
            m[8]  * m[1] * m[14] +
            m[8]  * m[2] * m[13] +
            m[12] * m[1] * m[10] -
            m[12] * m[2] * m[9];

        inv[2] = m[1]  * m[6] * m[15] -
            m[1]  * m[7] * m[14] -
            m[5]  * m[2] * m[15] +
            m[5]  * m[3] * m[14] +
            m[13] * m[2] * m[7] -
            m[13] * m[3] * m[6];

        inv[6] = -m[0]  * m[6] * m[15] +
            m[0]  * m[7] * m[14] +
            m[4]  * m[2] * m[15] -
            m[4]  * m[3] * m[14] -
            m[12] * m[2] * m[7] +
            m[12] * m[3] * m[6];

        inv[10] = m[0]  * m[5] * m[15] -
            m[0]  * m[7] * m[13] -
            m[4]  * m[1] * m[15] +
            m[4]  * m[3] * m[13] +
            m[12] * m[1] * m[7] -
            m[12] * m[3] * m[5];

        inv[14] = -m[0]  * m[5] * m[14] +
            m[0]  * m[6] * m[13] +
            m[4]  * m[1] * m[14] -
            m[4]  * m[2] * m[13] -
            m[12] * m[1] * m[6] +
            m[12] * m[2] * m[5];

        inv[3] = -m[1] * m[6] * m[11] +
            m[1] * m[7] * m[10] +
            m[5] * m[2] * m[11] -
            m[5] * m[3] * m[10] -
            m[9] * m[2] * m[7] +
            m[9] * m[3] * m[6];

        inv[7] = m[0] * m[6] * m[11] -
            m[0] * m[7] * m[10] -
            m[4] * m[2] * m[11] +
            m[4] * m[3] * m[10] +
            m[8] * m[2] * m[7] -
            m[8] * m[3] * m[6];

        inv[11] = -m[0] * m[5] * m[11] +
            m[0] * m[7] * m[9] +
            m[4] * m[1] * m[11] -
            m[4] * m[3] * m[9] -
            m[8] * m[1] * m[7] +
            m[8] * m[3] * m[5];

        inv[15] = m[0] * m[5] * m[10] -
            m[0] * m[6] * m[9] -
            m[4] * m[1] * m[10] +
            m[4] * m[2] * m[9] +
            m[8] * m[1] * m[6] -
            m[8] * m[2] * m[5];

        det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

        if (det == 0)
            return Mat4f::Indetity();

        det = 1.f / det;

        Mat4f res;
        for (i = 0; i < 16; i++)
            res.GetPtr()[i] = inv[i] * det;

        return res;
    }

public:

    // m_row_col; stored column major
    float m00, m10, m20, m30;
    float m01, m11, m21, m31;
    float m02, m12, m22, m32;
    float m03, m13, m23, m33;
};

//////////////////////////////////////////////////////////////////////////
// Frame

/// Coordinate frame
class Frame
{
public:

    Frame()
    {
        mX = Vec3f(1,0,0);
        mY = Vec3f(0,1,0);
        mZ = Vec3f(0,0,1);
    };

    Frame(
        const Vec3f& x,
        const Vec3f& y,
        const Vec3f& z
    ) :
        mX(x),
        mY(y),
        mZ(z)
    {}

    void SetFromZ(const Vec3f& z)
    {
        PG3_ASSERT_VEC3F_NORMALIZED(z);

        mZ = z;
        Vec3f tmpX = (std::abs(mZ.x) > 0.99f) ? Vec3f(0, 1, 0) : Vec3f(1, 0, 0);
        mY = Normalize(Cross(mZ, tmpX));
        mX = Cross(mY, mZ);
    }

    Vec3f ToWorld(const Vec3f& a) const
    {
        return mX * a.x + mY * a.y + mZ * a.z;
    }

    Vec3f ToLocal(const Vec3f& a) const
    {
        return Vec3f(Dot(a, mX), Dot(a, mY), Dot(a, mZ));
    }

    const Vec3f& Binormal() const { return mX; }
    const Vec3f& Tangent () const { return mY; }
    const Vec3f& Normal  () const { return mZ; }

public:

    Vec3f mX, mY, mZ;
};

