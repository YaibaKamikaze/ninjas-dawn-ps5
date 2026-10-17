// Ninja Gaiden II for PS5 - the Sony system calls NG2's PS5 code makes, as
// stubs for the Linux host test build (NG2_PS5_HOST_TEST in CMakeLists.txt).
// Never in a PS5 build. No controller (the game is driven by NG2_PAD_SCRIPT),
// silent audio paced like the real port, and "reload the app" exits.

#if defined(NG2_PS5_HOST_TEST)

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" {

int sceUserServiceInitialize(void*) { return 0; }
int sceUserServiceGetInitialUser(int32_t* user_id) {
  if (user_id) *user_id = 1;
  return 0;
}
int scePadInit(void) { return 0; }
int scePadOpen(int32_t, int32_t, int32_t, const void*) { return -1; }
int scePadGetHandle(int32_t, int32_t, int32_t) { return -1; }
int scePadReadState(int32_t, void*) { return -1; }

int32_t sceAudioOutInit(void) { return 0; }
int32_t sceAudioOutOpen(int32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t) { return 1; }
// A 256-sample frame at 48 kHz, like the console's port: the mixer thread
// takes its clock from this call.
int32_t sceAudioOutOutput(int32_t, const void* ptr) {
  if (ptr) std::this_thread::sleep_for(std::chrono::microseconds(5333));
  return 0;
}
int32_t sceAudioOutClose(int32_t) { return 0; }

int sceSystemServiceLoadExec(const char*, char* const*) {
  std::_Exit(0);
}

}  // extern "C"

#endif  // NG2_PS5_HOST_TEST
