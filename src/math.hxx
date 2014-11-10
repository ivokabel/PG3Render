#pragma once 

#include "debugging.hxx"
#include "types.hxx"

#include <cmath>
// for portability issues
#define PI_F     3.14159265358979f
#define INV_PI_F (1.f / PI_F)

template<typename T>
T Sqr(const T& a) { return a*a; }

float FmodX(float x, float y)
{
    float result = fmod(x, y);
    if (result < 0.0f)
        result += y;

    PG3_DEBUG_ASSERT_VAL_IN_RANGE(result, 0.0f, y);

    return result;
}

typedef unsigned uint;

//////////////////////////////////////////////////////////////////////////
// Math section

template <typename T>
inline T Clamp(const T& n, const T& lower, const T& upper)
{
    PG3_DEBUG_ASSERT(lower <= upper);
    return std::max(lower, std::min(n, upper));
}

template<typename T>
class Vec2x
{
public:

    Vec2x(){}
    Vec2x(T a):x(a),y(a){}
    Vec2x(T a, T b):x(a),y(b){}

    const T& Get(uint32_t a) const { return reinterpret_cast<const T*>(this)[a]; }
    T&       Get(uint32_t a)       { return reinterpret_cast<T*>(this)[a]; }

    // unary minus
    Vec2x<T> operator-() const
    { Vec2x<T> res; for (uint32_t i=0; i<2; i++) res.Get(i) = -Get(i); return res; }

    // binary operations
    friend Vec2x<T> operator+(const Vec2x& a, const Vec2x& b)
    { Vec2x<T> res; for (uint32_t i=0; i<2; i++) res.Get(i) = a.Get(i) + b.Get(i); return res; }
    friend Vec2x<T> operator-(const Vec2x& a, const Vec2x& b)
    { Vec2x<T> res; for (uint32_t i=0; i<2; i++) res.Get(i) = a.Get(i) - b.Get(i); return res; }
    friend Vec2x<T> operator*(const Vec2x& a, const Vec2x& b)
    { Vec2x<T> res; for (uint32_t i=0; i<2; i++) res.Get(i) = a.Get(i) * b.Get(i); return res; }
    friend Vec2x<T> operator/(const Vec2x& a, const Vec2x& b)
    { Vec2x<T> res; for (uint32_t i=0; i<2; i++) res.Get(i) = a.Get(i) / b.Get(i); return res; }

    Vec2x<T>& operator+=(const Vec2x& a)
    { for (uint32_t i=0; i<2; i++) Get(i) += a.Get(i); return *this;}
    Vec2x<T>& operator-=(const Vec2x& a)
    { for (uint32_t i=0; i<2; i++) Get(i) -= a.Get(i); return *this;}
    Vec2x<T>& operator*=(const Vec2x& a)
    { for (uint32_t i=0; i<2; i++) Get(i) *= a.Get(i); return *this;}
    Vec2x<T>& operator/=(const Vec2x& a)
    { for (uint32_t i=0; i<2; i++) Get(i) /= a.Get(i); return *this;}

    friend T Dot(const Vec2x& a, const Vec2x& b)
    { T res(0); for (uint32_t i=0; i<2; i++) res += a.Get(i) * b.Get(i); return res; }

public:

    T x, y;
};

typedef Vec2x<float>    Vec2f;
typedef Vec2x<int32_t>  Vec2i;
typedef Vec2x<uint32_t> Vec2ui;

template<typename T>
class Vec3x
{
public:

    Vec3x(){}
    Vec3x(T a):x(a),y(a),z(a){}
    Vec3x(T a, T b, T c):x(a),y(b),z(c){}

    const T& Get(uint32_t a) const { return reinterpret_cast<const T*>(this)[a]; }
    T&       Get(uint32_t a)       { return reinterpret_cast<T*>(this)[a]; }
    Vec2x<T> GetXY() const    { return Vec2x<T>(x, y); }
    T        Max()   const    { T res = Get(0); for (uint32_t i=1; i<3; i++) res = std::max(res, Get(i)); return res;}
    
    bool     IsZero() const
    {
        for (uint32_t i=0; i<3; i++)
            if (Get(i) != 0)
                return false;
        return true;
    }

    // unary minus
    Vec3x<T> operator-() const
    { Vec3x<T> res; for (uint32_t i=0; i<3; i++) res.Get(i) = -Get(i); return res; }

    // binary operations
    friend Vec3x<T> operator+(const Vec3x& a, const Vec3x& b)
    { Vec3x<T> res; for (uint32_t i=0; i<3; i++) res.Get(i) = a.Get(i) + b.Get(i); return res; }
    friend Vec3x<T> operator-(const Vec3x& a, const Vec3x& b)
    { Vec3x<T> res; for (uint32_t i=0; i<3; i++) res.Get(i) = a.Get(i) - b.Get(i); return res; }
    friend Vec3x<T> operator*(const Vec3x& a, const Vec3x& b)
    { Vec3x<T> res; for (uint32_t i=0; i<3; i++) res.Get(i) = a.Get(i) * b.Get(i); return res; }
    friend Vec3x<T> operator/(const Vec3x& a, const Vec3x& b)
    { Vec3x<T> res; for (uint32_t i=0; i<3; i++) res.Get(i) = a.Get(i) / b.Get(i); return res; }

    Vec3x<T>& operator+=(const Vec3x& a)
    { for (uint32_t i=0; i<3; i++) Get(i) += a.Get(i); return *this;}
    Vec3x<T>& operator-=(const Vec3x& a)
    { for (uint32_t i=0; i<3; i++) Get(i) -= a.Get(i); return *this;}
    Vec3x<T>& operator*=(const Vec3x& a)
    { for (uint32_t i=0; i<3; i++) Get(i) *= a.Get(i); return *this;}
    Vec3x<T>& operator/=(const Vec3x& a)
    { for (uint32_t i=0; i<3; i++) Get(i) /= a.Get(i); return *this;}

    friend T Dot(const Vec3x& a, const Vec3x& b)
    { T res(0); for (uint32_t i=0; i<3; i++) res += a.Get(i) * b.Get(i); return res; }

    float    LenSqr() const   { return Dot(*this, *this);   }
    float    Length() const   { return std::sqrt(LenSqr()); }

public:

    T x, y, z;
};

typedef Vec3x<float>    Vec3f;
typedef Vec3x<int32_t>  Vec3i;
typedef Vec3x<uint32_t> Vec3ui;

Vec3f Cross(
    const Vec3f &a,
    const Vec3f &b)
{
    Vec3f res;
    res.x = a.y * b.z - a.z * b.y;
    res.y = a.z * b.x - a.x * b.z;
    res.z = a.x * b.y - a.y * b.x;
    return res;
}

Vec3f Normalize(const Vec3f& a)
{
    const float lenSqr = Dot(a, a);
    const float len    = std::sqrt(lenSqr);
    return a / len;
}

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

    static Mat4f Zero() { Mat4f res(0); return res; }

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
        float aFov,
        float aNear,
        float aFar)
    {
        // Camera points towards -z.  0 < near < far.
        // Matrix maps z range [-near, -far] to [-1, 1], after homogeneous division.
        float f = 1.f / (std::tan(aFov * PI_F / 360.0f));
        float d = 1.f / (aNear - aFar);

        Mat4f r;
        r.m00 = f;    r.m01 = 0.0f; r.m02 = 0.0f;               r.m03 = 0.0f;
        r.m10 = 0.0f; r.m11 = -f;   r.m12 = 0.0f;               r.m13 = 0.0f;
        r.m20 = 0.0f; r.m21 = 0.0f; r.m22 = (aNear + aFar) * d; r.m23 = 2.0f * aNear * aFar * d;
        r.m30 = 0.0f; r.m31 = 0.0f; r.m32 = -1.0f;              r.m33 = 0.0f;

        return r;
    }
public:

    // m_row_col; stored column major
    float m00, m10, m20, m30;
    float m01, m11, m21, m31;
    float m02, m12, m22, m32;
    float m03, m13, m23, m33;
};

Mat4f operator*(const Mat4f& left, const Mat4f& right)
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
Mat4f Invert(const Mat4f& aMatrix)
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
        Vec3f tmpZ = mZ = Normalize(z);
        Vec3f tmpX = (std::abs(tmpZ.x) > 0.99f) ? Vec3f(0,1,0) : Vec3f(1,0,0);
        mY = Normalize( Cross(tmpZ, tmpX) );
        mX = Cross(mY, tmpZ);
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

