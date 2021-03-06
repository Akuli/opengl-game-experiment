#ifndef LINALG_HPP
#define LINALG_HPP

#include <array>
#include <cmath>  // IWYU pragma: keep

class vec2 {
public:
	float x, y;
	vec2() = default;
	vec2(float x, float y) : x(x), y(y) {};
	vec2(const std::array<float, 2> xy) : x(xy[0]), y(xy[1]) {}

	inline vec2 operator+(vec2 other) const { return vec2{this->x+other.x, this->y+other.y}; }
	inline vec2 operator-(vec2 other) const { return vec2{this->x-other.x, this->y-other.y}; }
	inline vec2 operator*(float f) const { return vec2{this->x*f, this->y*f}; }
	inline vec2 operator/(float f) const { return *this*(1/f); }
	inline vec2& operator+=(vec2 other) { *this = *this+other; return *this; }
	inline vec2& operator-=(vec2 other) { *this = *this-other; return *this; }
	inline vec2& operator*=(float f) { *this = *this*f; return *this; }
	inline vec2& operator/=(float f) { *this = *this/f; return *this; }
	inline vec2 operator-() { return { -this->x, -this->y }; }

	inline float dot(vec2 other) const { return this->x*other.x + this->y*other.y; }
	inline float length_squared() const { return this->dot(*this); }
	inline float length() const { return std::sqrt(this->length_squared()); }  // slow
	inline vec2 with_length(float new_length) const { return *this * (new_length/this->length()); }
	inline vec2 projection_to(vec2 v) const { return v * (this->dot(v)/v.dot(v)); }
};

class vec3 {
public:
	float x, y, z;
	vec3() = default;
	vec3(float x, float y, float z) : x(x), y(y), z(z) {};
	vec3(const std::array<float, 3> xyz) : x(xyz[0]), y(xyz[1]), z(xyz[2]) {}

	inline vec3 operator+(vec3 other) const { return vec3{this->x+other.x, this->y+other.y, this->z+other.z}; }
	inline vec3 operator-(vec3 other) const { return vec3{this->x-other.x, this->y-other.y, this->z-other.z}; }
	inline vec3 operator*(float f) const { return vec3{this->x*f, this->y*f, this->z*f}; }
	inline vec3 operator/(float f) const { return *this*(1/f); }
	inline vec3& operator+=(vec3 other) { *this = *this+other; return *this; }
	inline vec3& operator-=(vec3 other) { *this = *this-other; return *this; }
	inline vec3& operator*=(float f) { *this = *this*f; return *this; }
	inline vec3& operator/=(float f) { *this = *this/f; return *this; }
	inline vec3 operator-() { return { -this->x, -this->y, -this->z }; }

	inline vec3 cross(vec3 other) const { return vec3{ this->y*other.z - this->z*other.y, this->z*other.x - this->x*other.z, this->x*other.y - this->y*other.x }; }

	inline float dot(vec3 other) const { return this->x*other.x + this->y*other.y + this->z*other.z; }
	inline float length_squared() const { return this->dot(*this); }
	inline float length() const { return std::sqrt(this->length_squared()); }  // slow
	inline vec3 with_length(float new_length) const { return *this * (new_length/this->length()); }
	inline vec3 projection_to(vec3 v) const { return v * (this->dot(v)/v.dot(v)); }
};

class vec4 {
public:
	float x, y, z, w;
	vec4() = default;
	vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {};
	vec4(const std::array<float, 4> xyzw) : x(xyzw[0]), y(xyzw[1]), z(xyzw[2]), w(xyzw[3]) {}

	inline vec4 operator+(vec4 other) const { return vec4{this->x+other.x, this->y+other.y, this->z+other.z, this->w+other.w}; }
	inline vec4 operator-(vec4 other) const { return vec4{this->x-other.x, this->y-other.y, this->z-other.z, this->w-other.w}; }
	inline vec4 operator*(float f) const { return vec4{this->x*f, this->y*f, this->z*f, this->w*f}; }
	inline vec4 operator/(float f) const { return *this*(1/f); }
	inline vec4& operator+=(vec4 other) { *this = *this+other; return *this; }
	inline vec4& operator-=(vec4 other) { *this = *this-other; return *this; }
	inline vec4& operator*=(float f) { *this = *this*f; return *this; }
	inline vec4& operator/=(float f) { *this = *this/f; return *this; }
	inline vec4 operator-() { return { -this->x, -this->y, -this->z, -this->w }; }

	inline vec2 xy() const { return {x,y}; }
	inline vec2 zw() const { return {z,w}; }
	inline vec3 xyz() const { return {x,y,z}; }

	inline float dot(vec4 other) const { return this->x*other.x + this->y*other.y + this->z*other.z + this->w*other.w; }
	inline float length_squared() const { return this->dot(*this); }
	inline float length() const { return std::sqrt(this->length_squared()); }  // slow
	inline vec4 with_length(float new_length) const { return *this * (new_length/this->length()); }
	inline vec4 projection_to(vec4 v) const { return v * (this->dot(v)/v.dot(v)); }
};

class mat2 {
public:
	std::array<std::array<float, 2>, 2> rows;

	inline vec2 operator*(vec2 v) const
	{
		return vec2{
			vec2(this->rows[0]).dot(v),
			vec2(this->rows[1]).dot(v),
		};
	}

	static mat2 rotation(float angle);
};

class mat3 {
public:
	std::array<std::array<float, 3>, 3> rows;

	inline vec3 operator*(vec3 v) const
	{
		return vec3{
			vec3(this->rows[0]).dot(v),
			vec3(this->rows[1]).dot(v),
			vec3(this->rows[2]).dot(v),
		};
	}

	inline mat3 operator*(mat3 m) const
	{
		return mat3{
			vec3(this->rows[0]).dot(m.column(0)), vec3(this->rows[0]).dot(m.column(1)), vec3(this->rows[0]).dot(m.column(2)),
			vec3(this->rows[1]).dot(m.column(0)), vec3(this->rows[1]).dot(m.column(1)), vec3(this->rows[1]).dot(m.column(2)),
			vec3(this->rows[2]).dot(m.column(0)), vec3(this->rows[2]).dot(m.column(1)), vec3(this->rows[2]).dot(m.column(2)),
		};
	}

	static mat3 rotation_about_y(float angle);
	static mat3 rotation_about_z(float angle);
	float det() const;

	// Resulting matrix is a rotation that maps (0,1,0) to a unit vector in direction of v.
	static mat3 rotation_to_tilt_y_towards_vector(vec3 v);

private:
	inline vec3 column(int i) const { return vec3{this->rows[0][i], this->rows[1][i], this->rows[2][i]}; }
};

// should be safe to pass vector objects to opengl
static_assert(sizeof(vec2) == 2*sizeof(float));
static_assert(sizeof(vec3) == 3*sizeof(float));
static_assert(sizeof(vec4) == 4*sizeof(float));
static_assert(sizeof(mat2) == 2*2*sizeof(float));
static_assert(sizeof(mat3) == 3*3*sizeof(float));

class Plane {
public:
	// equation of plane represented as:  (x,y,z) dot normal = constant
	vec3 normal;
	float constant;

	void apply_matrix_INVERSE(mat3 inverse);
	void move(vec3 mv);
	inline bool whichside(vec3 v) const { return this->normal.dot(v) > this->constant; }
};

#endif
