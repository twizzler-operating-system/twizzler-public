#include <cstdio>
#include <utility>
#include <cstdint>
typedef __int128 guid_t;
typedef uint16_t lid_t;

template<typename ... T>
void test(char *fmt, T&& ... p)
{
	printf(fmt, std::forward<T>(p)...);
}

uintptr_t resolve_prefix(lid_t lid)
{
	printf("resolving prefix\n");
	if(lid == 0) return 0;
	return (uintptr_t)resolve_prefix;
}

template<typename T>
class Pointer {
private:
	lid_t lid;
	size_t offset;
	uintptr_t prefix;
	bool inited = false;
public:
	Pointer(int lid, size_t offset) : lid(lid), offset(offset)
	{}

	inline T& operator * () {
		if(!inited) {
			prefix = resolve_prefix(lid);
			inited = true;
		}
		return *(T *)(prefix + offset);
	}

	/*
	inline const T& operator * () const {
		if(!inited) {
			prefix = resolve_prefix(lid);
			inited = true;
		}
		return *(T *)(prefix + offset);
	}
	*/

	inline T * operator -> () {
		if(!inited) {
			prefix = resolve_prefix(lid);
			inited = true;
		}
		return (T *)(prefix + offset);
	}

	inline Pointer& operator += (long v) {
		offset += v * sizeof(T);
		return *this;
	}

	inline Pointer& operator -= (long v) {
		offset -= v * sizeof(T);
		return *this;
	}

	inline Pointer& operator ++ () {
		offset += sizeof(T);
	}
	
	inline Pointer& operator -- () {
		offset -= sizeof(T);
	}

	inline bool operator< (const Pointer& rhs) const {
		return this->lid != rhs.lid
			? false : this->offset < rhs.offset;
	}
	
	inline bool operator> (const Pointer& rhs) const { return rhs < *this; }
	inline bool operator<=(const Pointer& rhs) const { return !(*this > rhs); }
	inline bool operator>=(const Pointer& rhs) const { return !(*this < rhs); }

	inline bool operator==(const Pointer& rhs) const {
		return this->lid != rhs.lid
			? false : this->offset == rhs.offset;
	}

	inline bool operator!=(const Pointer& rhs) const { return !(*this == rhs); }

};

template<typename R,typename ... P>
class Function {
	private:
		lid_t lid;
		size_t offset;
		uintptr_t prefix;
		bool inited = false;
	public:
		Function(int lid, size_t offset) : lid(lid), offset(offset) {}

		R operator () (P&& ... params) {
			if(!inited) {
				prefix = resolve_prefix(lid);
				inited = true;
			}
			R (*ptr)(...) = (R (*)(...))(prefix + offset);
			return ptr(std::forward<P>(params)...);
		}
};

struct foo {
	int bar;
};

/* 
 * #define MAKE_POINTER(type, id, off) \
 * 	((type))((id) << 48 | (off))
 * int *baz = MAKE_POINTER(int *, 1, 0x1000);
 * baz;
 * prefix = resolve_prefix((uintptr_t)baz >> 48);
 * int *tmp = (int *)(prefix + (uintptr_t)baz & 0xFFFFFFFFFFFF);
 * *tmp
 */


int fn(int holy, char *shit)
{
	printf("fn! %d, %s\n", holy, shit);
	return 1234;
}

#define FOTE_FOO 1
#define FOTE_BAR 2

struct __fote {
	uint64_t flags, ad, nr, gr;
	union {
		char *name;
		unsigned __int128 guid;
	};
};

#define DEFAULT_RES_NAME 1234
#define DEFAULT_RES_GUID 5678

#define FOT_NAME (1 << 2) /* or whatever */

#define NAME(e, f, n, nr, gr, ad) \
	[e] = {flags : f | FOT_NAME, ad : ad, nr : nr, gr : gr, {.name = n}}

#define GUID(e, f, g, gr, ad) \
	[e] = {f & ~FOT_NAME, 0, gr, .guid = g}

#define FOT_ENTRIES struct __fote __twiz_foreign_object_table[]

FOT_ENTRIES = {
	NAME(FOTE_FOO, 0, "foo", DEFAULT_RES_NAME, 0, 0),
	GUID(FOTE_BAR, 0, 123456, DEFAULT_RES_GUID, 0),
};


int main()
{
	int lid=1;
	int offset = 0;
	Pointer<int> p(lid, offset);
	Pointer<int> q(lid, offset+4);
	test("hello %d\n", 5);

	printf(":: %d\n", *p);
	printf(":: %d\n", *p);

	Pointer<int> s(lid+1, offset+4);

	printf("%d %d %d\n", s >= p, s <= p, s == p);
	printf("%d %d %d\n", q >= p, q <= p, q == p);

	Function<int, int, char *> f(0, (uintptr_t)fn);
	int r = f(5, "batman");
	printf("Got %d\n", r);
}

