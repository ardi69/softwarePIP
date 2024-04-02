#ifndef tools_h__
#define tools_h__

#include <cstdlib>
//#include <stddef.h>
#include <cstdint>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <io.h>
#define aligned_alloc(align, size) _aligned_malloc(size, align)
#define aligned_free _aligned_free
#define read _read
#else
#define aligned_free free
#include <unistd.h>
#endif
#include <cerrno>

#define _log(a, fmt, ...) (fprintf(stderr, a), fprintf(stderr, fmt, ##__VA_ARGS__), fprintf(stderr, "\n"))
#define esyslog(fmt, ...) _log("[error] ", fmt, ##__VA_ARGS__)
#if _MSC_VER && 1
#define isyslog(fmt, ...) _log("[info]  ", fmt, ##__VA_ARGS__)
#define dsyslog(fmt, ...) _log("[debug] ", fmt, ##__VA_ARGS__)
#else
#define isyslog(...) (0)
#define dsyslog(...) (0)
#endif

#define KILOBYTE(n) ((n) * 1024)
#define MEGABYTE(n) ((n) * 1024LL * 1024LL)

#define MALLOC(type, size) (type *)malloc(sizeof(type) * (size))
#define ALIGNED_ALLOC(type, align, size) (type *)aligned_alloc(align, sizeof(type) * (size))
#define ALIGNED_FREE(p) aligned_free(p)

template<class T> inline void DELETENULL(T *&p) { T *q = p; p = NULL; delete q; }

static inline int safe_read(int FileHandle, uint8_t *buffer, int size) {
	for (;;) {
		int p = read(FileHandle, buffer, size);
		if (p < 0 && errno == EINTR) {
			dsyslog("EINTR while reading from file handle %d - retrying", FileHandle);
			continue;
		}
		return p;
	}
}

enum ePictureType {
	NO_PICTURE = 0,
	I_FRAME,
#define SI_FRAME I_FRAME
	P_FRAME,
#define SP_FRAME P_FRAME
	B_FRAME,
};

#include <condition_variable>
#include <mutex>
#include <chrono>

class cEvent {
public:
	cEvent() : flag(false) {}
	void Signal() {
		std::unique_lock<std::mutex> lck(mtx);
		flag = true;
		con.notify_one();
	}
	void SignalAll() {
		std::unique_lock<std::mutex> lck(mtx);
		flag = true;
		con.notify_all();
	}
	void Wait() {
		std::unique_lock<std::mutex> lck(mtx);
		if (!flag) con.wait(lck);
		flag = false;
	}
	template<class _Rep, class _Period>
	bool Wait(const std::chrono::duration<_Rep, _Period> &wait_time) {
		std::unique_lock<std::mutex> lck(mtx);
		if (!flag) {
			if (con.wait_for(lck, wait_time) == std::cv_status::timeout)
				return false;
		}
		flag = false;
		return true;
	}
	bool Wait(double seconds) {
		std::unique_lock<std::mutex> lck(mtx);
		if (!flag) {
			if (con.wait_for(lck, std::chrono::microseconds((long long)(seconds * 1000000))) == std::cv_status::timeout)
				return false;
		}
		flag = false;
		return true;
	}
	bool Wait_ms(unsigned long milliseconds) {
		std::unique_lock<std::mutex> lck(mtx);
		if (!flag)
			if (con.wait_for(lck, std::chrono::milliseconds(milliseconds)) == std::cv_status::timeout)
				return false;
		flag = false;
		return true;
	}
private:
	std::mutex				mtx;
	std::condition_variable	con;
	bool					flag;
};


#endif // tools_h__