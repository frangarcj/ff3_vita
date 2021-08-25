/* main.c -- Final Fantasy 3 .so loader
 *
 * Copyright (C) 2021 Andy Nguyen
 * Copyright (C) 2021 Francisco José García García
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <kubridge.h>
#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/ctrl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/touch.h>
#include <vitaGL.h>
#include <vitashark.h>

#include <OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <malloc.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <math.h>

#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "bridge.h"
#include "config.h"
#include "dialog.h"
#include "main.h"
#include "so_util.h"

int SCREEN_W = DEF_SCREEN_W;
int SCREEN_H = DEF_SCREEN_H;

config_opts options;
void loadOptions() {
  char buffer[30];
  int value;

  FILE *f = fopen(CONFIG_FILE_PATH, "rb");
  if (f) {
    while (EOF != fscanf(f, "%[^=]=%d\n", buffer, &value)) {
      if (strcmp("resolution", buffer) == 0)
        options.res = value;
      else if (strcmp("bilinear", buffer) == 0)
        options.bilinear = value;
      else if (strcmp("language", buffer) == 0)
        options.lang = value;
      else if (strcmp("antialiasing", buffer) == 0)
        options.msaa = value;
    }
  } else {
    options.res = 544;
    options.bilinear = 0;
    options.lang = 0;
    options.msaa = 2;
  }

  switch (options.res) {
  case 544:
    SCREEN_W = 960;
    SCREEN_H = 544;
    break;
  case 720:
    SCREEN_W = 1280;
    SCREEN_H = 725;
    break;
  case 1080:
    SCREEN_W = 1080;
    SCREEN_H = 1088;
    break;
  }
}

int _newlib_heap_size_user = MEMORY_NEWLIB_MB * 1024 * 1024;

int _opensles_user_freq = 32000;

static so_module ff3_mod;

void *__wrap_memcpy(void *dest, const void *src, size_t n) {
  return sceClibMemcpy(dest, src, n);
}

void *__wrap_memmove(void *dest, const void *src, size_t n) {
  return sceClibMemmove(dest, src, n);
}

void *__wrap_memset(void *s, int c, size_t n) { return sceClibMemset(s, c, n); }

int ret0(void) { return 0; }

int ret1(void) { return 1; }

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
#ifdef DEBUG
  va_list list;
  char string[512];

  va_start(list, fmt);
  vsprintf(string, fmt, list);
  va_end(list);

  printf("%s\n", string);
#endif
  return 0;
}

void __assert2(const char *file, int line, const char *func, const char *expr) {
  printf("assertion failed:\n%s:%d (%s): %s\n", file, line, func, expr);
}

static const short _C_toupper_[] = {
    -1,   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22,
    0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
    0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
    0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
    0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
    0x5f, 0x60, 'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',
    'K',  'L',  'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',  'U',  'V',
    'W',  'X',  'Y',  'Z',  0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82,
    0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
    0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
    0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
    0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2,
    0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
    0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
    0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
    0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
    0xfb, 0xfc, 0xfd, 0xfe, 0xff};

const short *_toupper_tab_ = _C_toupper_;

enum MethodIDs {
  UNKNOWN = 0,
  GET_CURRENT_FRAME, /**/
  LOAD_FILE,         /**/
  LOAD_RAW_FILE,
  GET_LANGUAGE,
  GET_SAVEFILENAME,
  CREATE_SAVEFILE,
  LOAD_TEXTURE,
  IS_DEVICE_ANDROID_TV,
  DRAW_FONT,
  CREATE_EDIT_TEXT,
  GET_EDIT_TEXT,
  START_DOWNLOAD,
} MethodIDs;

typedef struct {
  char *name;
  enum MethodIDs id;
} NameToMethodID;

static NameToMethodID name_to_method_ids[] = {
    {"getCurrentFrame", GET_CURRENT_FRAME},
    {"loadFile", LOAD_FILE},
    {"loadRawFile", LOAD_RAW_FILE},
    {"getLanguage", GET_LANGUAGE},
    {"getSaveFileName", GET_SAVEFILENAME},
    {"createSaveFile", CREATE_SAVEFILE},
    {"loadTexture", LOAD_TEXTURE},
    {"isDeviceAndroidTV", IS_DEVICE_ANDROID_TV},
    {"drawFont", DRAW_FONT},
    {"createEditText", CREATE_EDIT_TEXT},
    {"getEditText", GET_EDIT_TEXT},
    {"startDownload", START_DOWNLOAD}};

int GetMethodID(void *env, void *class, const char *name, const char *sig) {

  for (int i = 0; i < sizeof(name_to_method_ids) / sizeof(NameToMethodID);
       i++) {
    if (strcmp(name, name_to_method_ids[i].name) == 0)
      return name_to_method_ids[i].id;
  }
  printf("GetMethodID %s\n", name);
  return UNKNOWN;
}

int GetStaticMethodID(void *env, void *class, const char *name,
                      const char *sig) {

  for (int i = 0; i < sizeof(name_to_method_ids) / sizeof(NameToMethodID);
       i++) {
    if (strcmp(name, name_to_method_ids[i].name) == 0)
      return name_to_method_ids[i].id;
  }
  printf("GetMethodID %s\n", name);

  return UNKNOWN;
}

int CallBooleanMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  switch (methodID) {
  default:
    return 0;
  }
}

float CallFloatMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  return 0.0f;
}

int CallIntMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  return 0;
}

void *CallObjectMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  return NULL;
}

void CallVoidMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  return;
}

void *CallStaticObjectMethodV(void *env, void *obj, int methodID,
                              uintptr_t *args) {
  switch (methodID) {
  case LOAD_FILE:
    return loadFile((char *)args[0]);
  case LOAD_RAW_FILE:
    return loadRawFile((char *)args[0]);
  case GET_SAVEFILENAME:
    return getSaveFileName();
  case LOAD_TEXTURE:
    return loadTexture((jni_bytearray *)args[0]);
  case DRAW_FONT:
    return drawFont((char *)args[0], args[1], args[2], args[3]);
  case GET_EDIT_TEXT:
    return getEditText();
  default:
    return NULL;
  }
}

void CallStaticVoidMethodV(void *env, void *obj, int methodID,
                           uintptr_t *args) {
  switch (methodID) {
  case CREATE_SAVEFILE:
    createSaveFile((size_t)args[0]);
    break;
  case CREATE_EDIT_TEXT:
    createEditText((char *)args[0]);
    break;
  case START_DOWNLOAD:
    loadCompanionApp();
    break;
  default:
    return;
  }
}

int CallStaticBooleanMethodV(void *env, void *obj, int methodID,
                             uintptr_t *args) {
  switch (methodID) {
  case IS_DEVICE_ANDROID_TV:
    return isDeviceAndroidTV();
  default:
    return 0;
  }
}

uint64_t CallStaticLongMethodV(void *env, void *obj, int methodID,
                               uintptr_t *args) {
  switch (methodID) {
  case GET_CURRENT_FRAME:
    return getCurrentFrame((uint64_t)args[0]);
  default:
    return 0;
  }
}

int CallStaticIntMethodV(void *env, void *obj, int methodID, uintptr_t *args) {
  switch (methodID) {
  case GET_LANGUAGE:
    return getCurrentLanguage();
  default:
    return 0;
  }
}

float CallStaticFloatMethodV(void *env, void *obj, int methodID,
                             uintptr_t *args) {
  switch (methodID) {
  default:
    return 0.0f;
  }
}

void *FindClass(void) { return (void *)0x41414141; }

void DeleteLocalRef(void *env, void *localRef) { return; }

void *NewObjectV(void *env, void *clazz, int methodID, va_list args) {
  return NULL;
}

int GetStaticFieldID(void *env, void *clazz, const char *name,
                     const char *sig) {
  return 0;
}

void *GetStaticObjectField(void *clazz, int fieldID) { return ""; }

char *NewStringUTF(void *env, char *bytes) { return bytes; }

char *GetStringUTFChars(void *env, char *string, int *isCopy) { return string; }

int GetStringUTFLength(void *env, char *string) { return strlen(string); }

void GetStringUTFRegion(void *env, char *str, int start, int len, char *buf) {
  return;
}

int GetFieldID(void *env, void *clazz, const char *name, const char *sig) {
  return 0;
}

int GetFloatField(void *env, void *obj, int fieldID) { return 0; }

int GetArrayLength(void *env, jni_bytearray *obj) { return obj->size; }

void *GetByteArrayElements(void *env, jni_bytearray *obj) {
  return obj->elements;
}

void *NewByteArray(void *env, size_t length) {
  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = malloc(length);
  result->size = length;
  return result;
}

void *GetIntArrayElements(void *env, jni_intarray *obj) {
  return obj->elements;
}

int ReleaseByteArrayElements(void *env, jni_bytearray *obj) {
  free(obj->elements);
  free(obj);
  return 0;
}

int ReleaseIntArrayElements(void *env, jni_intarray *obj) {
  free(obj->elements);
  free(obj);
  return 0;
}

void SetByteArrayRegion(void *env, jni_bytearray *array, size_t start,
                        size_t len, const unsigned char *buf) {
  memcpy(array->elements, &buf[start], len);
}

static char fake_env[0x1000];

void InitJNIEnv(void) {
  memset(fake_env, 'A', sizeof(fake_env));
  *(uintptr_t *)(fake_env + 0x00) =
      (uintptr_t)fake_env; // just point to itself...
  *(uintptr_t *)(fake_env + 0x18) = (uintptr_t)FindClass;
  // *(uintptr_t *)(fake_env + 0x44) = (uintptr_t)ret0; // ExceptionClear
  // *(uintptr_t *)(fake_env + 0x54) = (uintptr_t)NewGlobalRef;
  *(uintptr_t *)(fake_env + 0x5C) = (uintptr_t)DeleteLocalRef;
  *(uintptr_t *)(fake_env + 0x74) = (uintptr_t)NewObjectV;
  *(uintptr_t *)(fake_env + 0x84) = (uintptr_t)GetMethodID;
  *(uintptr_t *)(fake_env + 0x8C) = (uintptr_t)CallObjectMethodV;
  *(uintptr_t *)(fake_env + 0x98) = (uintptr_t)CallBooleanMethodV;
  *(uintptr_t *)(fake_env + 0xC8) = (uintptr_t)CallIntMethodV;
  *(uintptr_t *)(fake_env + 0xE0) = (uintptr_t)CallFloatMethodV;
  *(uintptr_t *)(fake_env + 0xF8) = (uintptr_t)CallVoidMethodV;
  *(uintptr_t *)(fake_env + 0x178) = (uintptr_t)GetFieldID;
  *(uintptr_t *)(fake_env + 0x198) = (uintptr_t)GetFloatField;
  *(uintptr_t *)(fake_env + 0x1C4) = (uintptr_t)GetStaticMethodID;
  *(uintptr_t *)(fake_env + 0x1CC) = (uintptr_t)CallStaticObjectMethodV;
  *(uintptr_t *)(fake_env + 0x1D8) = (uintptr_t)CallStaticBooleanMethodV;
  *(uintptr_t *)(fake_env + 0x208) = (uintptr_t)CallStaticIntMethodV;
  *(uintptr_t *)(fake_env + 0x214) = (uintptr_t)CallStaticLongMethodV;
  *(uintptr_t *)(fake_env + 0x220) = (uintptr_t)CallStaticFloatMethodV;
  *(uintptr_t *)(fake_env + 0x238) = (uintptr_t)CallStaticVoidMethodV;
  *(uintptr_t *)(fake_env + 0x240) = (uintptr_t)GetStaticFieldID;
  *(uintptr_t *)(fake_env + 0x244) = (uintptr_t)GetStaticObjectField;
  *(uintptr_t *)(fake_env + 0x29C) = (uintptr_t)NewStringUTF;
  *(uintptr_t *)(fake_env + 0x2A0) = (uintptr_t)GetStringUTFLength;
  *(uintptr_t *)(fake_env + 0x2A4) = (uintptr_t)GetStringUTFChars;
  *(uintptr_t *)(fake_env + 0x2A8) = (uintptr_t)ret0; // ReleaseStringUTFChars
  *(uintptr_t *)(fake_env + 0x2AC) = (uintptr_t)GetArrayLength;
  *(uintptr_t *)(fake_env + 0x2E0) = (uintptr_t)GetByteArrayElements;
  *(uintptr_t *)(fake_env + 0x2C0) = (uintptr_t)NewByteArray;
  *(uintptr_t *)(fake_env + 0x2EC) = (uintptr_t)GetIntArrayElements;
  *(uintptr_t *)(fake_env + 0x300) = (uintptr_t)ReleaseByteArrayElements;
  *(uintptr_t *)(fake_env + 0x30C) = (uintptr_t)ReleaseIntArrayElements;
  *(uintptr_t *)(fake_env + 0x340) = (uintptr_t)SetByteArrayRegion;

  // *(uintptr_t *)(fake_env + 0x35C) = (uintptr_t)RegisterNatives;
  *(uintptr_t *)(fake_env + 0x374) = (uintptr_t)GetStringUTFRegion;
}

void *Android_JNI_GetEnv(void) { return fake_env; }

int pthread_mutex_init_fake(pthread_mutex_t **uid,
                            const pthread_mutexattr_t *mutexattr) {
  pthread_mutex_t *m = calloc(1, sizeof(pthread_mutex_t));
  if (!m)
    return -1;

  const int recursive = (mutexattr && *(const int *)mutexattr == 1);
  *m = recursive ? PTHREAD_RECURSIVE_MUTEX_INITIALIZER
                 : PTHREAD_MUTEX_INITIALIZER;

  int ret = pthread_mutex_init(m, mutexattr);
  if (ret < 0) {
    free(m);
    return -1;
  }

  *uid = m;

  return 0;
}

int pthread_mutex_destroy_fake(pthread_mutex_t **uid) {
  if (uid && *uid && (uintptr_t)*uid > 0x8000) {
    pthread_mutex_destroy(*uid);
    free(*uid);
    *uid = NULL;
  }
  return 0;
}

int pthread_mutex_lock_fake(pthread_mutex_t **uid) {
  int ret = 0;
  if (!*uid) {
    ret = pthread_mutex_init_fake(uid, NULL);
  } else if ((uintptr_t)*uid == 0x4000) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    ret = pthread_mutex_init_fake(uid, &attr);
    pthread_mutexattr_destroy(&attr);
  } else if ((uintptr_t)*uid == 0x8000) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    ret = pthread_mutex_init_fake(uid, &attr);
    pthread_mutexattr_destroy(&attr);
  }
  if (ret < 0)
    return ret;
  return pthread_mutex_lock(*uid);
}

int pthread_mutex_unlock_fake(pthread_mutex_t **uid) {
  int ret = 0;
  if (!*uid) {
    ret = pthread_mutex_init_fake(uid, NULL);
  } else if ((uintptr_t)*uid == 0x4000) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    ret = pthread_mutex_init_fake(uid, &attr);
    pthread_mutexattr_destroy(&attr);
  } else if ((uintptr_t)*uid == 0x8000) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    ret = pthread_mutex_init_fake(uid, &attr);
    pthread_mutexattr_destroy(&attr);
  }
  if (ret < 0)
    return ret;
  return pthread_mutex_unlock(*uid);
}

int pthread_cond_init_fake(pthread_cond_t **cnd, const int *condattr) {
  pthread_cond_t *c = calloc(1, sizeof(pthread_cond_t));
  if (!c)
    return -1;

  *c = PTHREAD_COND_INITIALIZER;

  int ret = pthread_cond_init(c, NULL);
  if (ret < 0) {
    free(c);
    return -1;
  }

  *cnd = c;

  return 0;
}

int pthread_cond_broadcast_fake(pthread_cond_t **cnd) {
  if (!*cnd) {
    if (pthread_cond_init_fake(cnd, NULL) < 0)
      return -1;
  }
  return pthread_cond_broadcast(*cnd);
}

int pthread_cond_signal_fake(pthread_cond_t **cnd) {
  if (!*cnd) {
    if (pthread_cond_init_fake(cnd, NULL) < 0)
      return -1;
  }
  return pthread_cond_signal(*cnd);
}

int pthread_cond_destroy_fake(pthread_cond_t **cnd) {
  if (cnd && *cnd) {
    pthread_cond_destroy(*cnd);
    free(*cnd);
    *cnd = NULL;
  }
  return 0;
}

int pthread_cond_wait_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx) {
  if (!*cnd) {
    if (pthread_cond_init_fake(cnd, NULL) < 0)
      return -1;
  }
  return pthread_cond_wait(*cnd, *mtx);
}

int pthread_cond_timedwait_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx,
                                const struct timespec *t) {
  if (!*cnd) {
    if (pthread_cond_init_fake(cnd, NULL) < 0)
      return -1;
  }
  return pthread_cond_timedwait(*cnd, *mtx, t);
}

int pthread_create_fake(pthread_t *thread, const void *unused, void *entry,
                        void *arg) {
  return pthread_create(thread, NULL, entry, arg);
}

int pthread_once_fake(volatile int *once_control, void (*init_routine)(void)) {
  if (!once_control || !init_routine)
    return -1;
  if (__sync_lock_test_and_set(once_control, 1) == 0)
    (*init_routine)();
  return 0;
}

int gettid(void) { return sceKernelGetThreadId(); }

char *getcwd(char *buf, size_t size) {
  if (buf) {
    buf[0] = '\0';
    return buf;
  }
  return NULL;
}

int this_width;
int this_height;

void setup_viewport(int width, int height) {
  int i3 = width * 3;
  if (i3 >= height * 4) {
    this_height = height;
    this_width = width;
    if (this_width > (this_height * 21) / 9) {
      this_width = (this_height * 21) / 9;
    }
  } else {
    this_width = width;
    this_height = i3 / 4;
  }
  int x = (width - this_width) / 2;
  int y = (height - this_height) / 2;
  glViewport(x, y, this_width, this_height);
}

int main_thread(SceSize args, void *argp) {

  int has_low_res;

  switch (options.msaa) {
  case 0:
    has_low_res = vglInitExtended(0, SCREEN_W, SCREEN_H,
                                  MEMORY_VITAGL_THRESHOLD_MB * 1024 * 1024,
                                  SCE_GXM_MULTISAMPLE_NONE);
    break;
  case 1:
    has_low_res = vglInitExtended(0, SCREEN_W, SCREEN_H,
                                  MEMORY_VITAGL_THRESHOLD_MB * 1024 * 1024,
                                  SCE_GXM_MULTISAMPLE_2X);
    break;
  default:
    has_low_res = vglInitExtended(0, SCREEN_W, SCREEN_H,
                                  MEMORY_VITAGL_THRESHOLD_MB * 1024 * 1024,
                                  SCE_GXM_MULTISAMPLE_4X);
    break;
  }

  int (*ff3_render)(char *, int, int, int, int) =
      (void *)so_symbol(&ff3_mod, "render");
  int (*ff3_touch)(int, int, int, int, float, float, float, float,
                   unsigned int) = (void *)so_symbol(&ff3_mod, "touch");

  readHeader();
  setup_viewport(has_low_res ? DEF_SCREEN_W : SCREEN_W,
                 has_low_res ? DEF_SCREEN_H : SCREEN_H);
  while (1) {

    SceTouchData touch;
    float coordinates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int n;

    sceTouchPeek(0, &touch, 1);

    int reportNum = touch.reportNum > 2 ? 2 : touch.reportNum;

    for (n = 0; n < reportNum; n++) {
      coordinates[n * 2] = (float)touch.report[n].x / 1920.0f;
      coordinates[n * 2 + 1] = (float)touch.report[n].y / 1088.0f;
    }

    SceCtrlData pad;
    sceCtrlPeekBufferPositiveExt2(0, &pad, 1);

    int mask = 0;

    if (pad.buttons & SCE_CTRL_TRIANGLE)
      mask |= 0x400;
    if (pad.buttons & SCE_CTRL_SQUARE)
      mask |= 0x200;
    if (pad.buttons & SCE_CTRL_L1)
      mask |= 0x100;
    if (pad.buttons & SCE_CTRL_R1)
      mask |= 0x800;
    if (pad.buttons & SCE_CTRL_CROSS)
      mask |= 0x1;
    if (pad.buttons & SCE_CTRL_CIRCLE)
      mask |= 0x4000;
    if (pad.buttons & SCE_CTRL_START)
      mask |= 0x8;
    if (pad.buttons & SCE_CTRL_SELECT)
      mask |= 0x4;
    if (pad.buttons & SCE_CTRL_UP || pad.ly < 80)
      mask |= 0x40;
    if (pad.buttons & SCE_CTRL_DOWN || pad.ly > 170)
      mask |= 0x80;
    if (pad.buttons & SCE_CTRL_LEFT || pad.lx < 80)
      mask |= 0x20;
    if (pad.buttons & SCE_CTRL_RIGHT || pad.lx > 170)
      mask |= 0x10;

    ff3_touch(0, 0, reportNum, reportNum, coordinates[0], coordinates[1],
              coordinates[2], coordinates[3], mask);

    ff3_render(fake_env, 0, this_width, this_height, 0);
    vglSwapBuffers(GL_FALSE);
  }

  return 0;
}

void patch_game(void) {
#ifdef DEBUG
  hook_thumb(ff3_mod.text_base + 0xe58b6, (uintptr_t)&printf);
#endif
}

extern void *_ZdaPv;
extern void *_ZdlPv;
extern void *_Znaj;
extern void *_Znwj;

extern void *__aeabi_atexit;
extern void *__aeabi_d2ulz;
extern void *__aeabi_dcmpgt;
extern void *__aeabi_dmul;
extern void *__aeabi_f2d;
extern void *__aeabi_fadd;
extern void *__aeabi_fsub;
extern void *__aeabi_idiv;
extern void *__aeabi_idivmod;
extern void *__aeabi_l2d;
extern void *__aeabi_l2f;
extern void *__aeabi_ldivmod;
extern void *__aeabi_uidiv;
extern void *__aeabi_uidivmod;
extern void *__aeabi_uldivmod;
extern void *__cxa_atexit;
extern void *__cxa_finalize;
extern void *__stack_chk_fail;

static int __stack_chk_guard_fake = 0x42424242;
static FILE __sF_fake[0x100][3];

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset) {
  return malloc(length);
}

int munmap(void *addr, size_t length) {
  free(addr);
  return 0;
}

void *AAssetManager_open(void *mgr, const char *filename, int mode) {
  printf("AAssetManager_open\n");
  return NULL;
}

void *AAsset_close() {
  printf("AAsset_close\n");
  return NULL;
}

void *AAssetManager_fromJava() {
  printf("AAssetManager_fromJava\n");
  return NULL;
}

void *AAsset_read() {
  printf("AAsset_read\n");
  return NULL;
}

void *AAsset_seek() {
  printf("AAsset_seek\n");
  return NULL;
}

void *AAsset_getLength() {
  printf("AAsset_getLength\n");
  return NULL;
}

void glTexParameteriHook(GLenum target, GLenum pname, GLint param) {
  if (options.bilinear) {
    if (pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER) {
      glTexParameteri(target, pname, GL_LINEAR);
      return;
    }
  }
  glTexParameteri(target, pname, param);
}

static so_default_dynlib default_dynlib[] = {
    {"AAssetManager_open", (uintptr_t)&AAssetManager_open},
    {"AAsset_close", (uintptr_t)&AAsset_close},
    {"AAssetManager_fromJava", (uintptr_t)&AAssetManager_fromJava},
    {"AAsset_read", (uintptr_t)&AAsset_read},
    {"AAsset_seek", (uintptr_t)&AAsset_seek},
    {"AAsset_getLength", (uintptr_t)&AAsset_getLength},
    {"SL_IID_ANDROIDSIMPLEBUFFERQUEUE",
     (uintptr_t)&SL_IID_ANDROIDSIMPLEBUFFERQUEUE},
    {"SL_IID_AUDIOIODEVICECAPABILITIES",
     (uintptr_t)&SL_IID_AUDIOIODEVICECAPABILITIES},
    {"SL_IID_BUFFERQUEUE", (uintptr_t)&SL_IID_BUFFERQUEUE},
    {"SL_IID_DYNAMICSOURCE", (uintptr_t)&SL_IID_DYNAMICSOURCE},
    {"SL_IID_ENGINE", (uintptr_t)&SL_IID_ENGINE},
    {"SL_IID_LED", (uintptr_t)&SL_IID_LED},
    {"SL_IID_NULL", (uintptr_t)&SL_IID_NULL},
    {"SL_IID_METADATAEXTRACTION", (uintptr_t)&SL_IID_METADATAEXTRACTION},
    {"SL_IID_METADATATRAVERSAL", (uintptr_t)&SL_IID_METADATATRAVERSAL},
    {"SL_IID_OBJECT", (uintptr_t)&SL_IID_OBJECT},
    {"SL_IID_OUTPUTMIX", (uintptr_t)&SL_IID_OUTPUTMIX},
    {"SL_IID_PLAY", (uintptr_t)&SL_IID_PLAY},
    {"SL_IID_VIBRA", (uintptr_t)&SL_IID_VIBRA},
    {"SL_IID_VOLUME", (uintptr_t)&SL_IID_VOLUME},
    {"SL_IID_PREFETCHSTATUS", (uintptr_t)&SL_IID_PREFETCHSTATUS},
    {"SL_IID_PLAYBACKRATE", (uintptr_t)&SL_IID_PLAYBACKRATE},
    {"SL_IID_SEEK", (uintptr_t)&SL_IID_SEEK},
    {"SL_IID_RECORD", (uintptr_t)&SL_IID_RECORD},
    {"SL_IID_EQUALIZER", (uintptr_t)&SL_IID_EQUALIZER},
    {"SL_IID_DEVICEVOLUME", (uintptr_t)&SL_IID_DEVICEVOLUME},
    {"SL_IID_PRESETREVERB", (uintptr_t)&SL_IID_PRESETREVERB},
    {"SL_IID_ENVIRONMENTALREVERB", (uintptr_t)&SL_IID_ENVIRONMENTALREVERB},
    {"SL_IID_EFFECTSEND", (uintptr_t)&SL_IID_EFFECTSEND},
    {"SL_IID_3DGROUPING", (uintptr_t)&SL_IID_3DGROUPING},
    {"SL_IID_3DCOMMIT", (uintptr_t)&SL_IID_3DCOMMIT},
    {"SL_IID_3DLOCATION", (uintptr_t)&SL_IID_3DLOCATION},
    {"SL_IID_3DDOPPLER", (uintptr_t)&SL_IID_3DDOPPLER},
    {"SL_IID_3DSOURCE", (uintptr_t)&SL_IID_3DSOURCE},
    {"SL_IID_3DMACROSCOPIC", (uintptr_t)&SL_IID_3DMACROSCOPIC},
    {"SL_IID_MUTESOLO", (uintptr_t)&SL_IID_MUTESOLO},
    {"SL_IID_DYNAMICINTERFACEMANAGEMENT",
     (uintptr_t)&SL_IID_DYNAMICINTERFACEMANAGEMENT},
    {"SL_IID_MIDIMESSAGE", (uintptr_t)&SL_IID_MIDIMESSAGE},
    {"SL_IID_MIDIMUTESOLO", (uintptr_t)&SL_IID_MIDIMUTESOLO},
    {"SL_IID_MIDITEMPO", (uintptr_t)&SL_IID_MIDITEMPO},
    {"SL_IID_MIDITIME", (uintptr_t)&SL_IID_MIDITIME},
    {"SL_IID_AUDIODECODERCAPABILITIES",
     (uintptr_t)&SL_IID_AUDIODECODERCAPABILITIES},
    {"SL_IID_AUDIOENCODERCAPABILITIES",
     (uintptr_t)&SL_IID_AUDIOENCODERCAPABILITIES},
    {"SL_IID_AUDIOENCODER", (uintptr_t)&SL_IID_AUDIOENCODER},
    {"SL_IID_BASSBOOST", (uintptr_t)&SL_IID_BASSBOOST},
    {"SL_IID_PITCH", (uintptr_t)&SL_IID_PITCH},
    {"SL_IID_RATEPITCH", (uintptr_t)&SL_IID_RATEPITCH},
    {"SL_IID_VIRTUALIZER", (uintptr_t)&SL_IID_VIRTUALIZER},
    {"SL_IID_VISUALIZATION", (uintptr_t)&SL_IID_VISUALIZATION},
    {"SL_IID_ENGINECAPABILITIES", (uintptr_t)&SL_IID_ENGINECAPABILITIES},
    {"SL_IID_THREADSYNC", (uintptr_t)&SL_IID_THREADSYNC},
    {"SL_IID_ANDROIDEFFECT", (uintptr_t)&SL_IID_ANDROIDEFFECT},
    {"SL_IID_ANDROIDEFFECTSEND", (uintptr_t)&SL_IID_ANDROIDEFFECTSEND},
    {"SL_IID_ANDROIDEFFECTCAPABILITIES",
     (uintptr_t)&SL_IID_ANDROIDEFFECTCAPABILITIES},
    {"SL_IID_ANDROIDCONFIGURATION", (uintptr_t)&SL_IID_ANDROIDCONFIGURATION},
    {"_ZdaPv", (uintptr_t)&_ZdaPv},
    {"_ZdlPv", (uintptr_t)&_ZdlPv},
    {"_Znaj", (uintptr_t)&_Znaj},
    {"_Znwj", (uintptr_t)&_Znwj},
    {"_toupper_tab_", (uintptr_t)&_toupper_tab_},
    {"__aeabi_atexit", (uintptr_t)&__aeabi_atexit},
    {"__aeabi_d2ulz", (uintptr_t)&__aeabi_d2ulz},
    {"__aeabi_dcmpgt", (uintptr_t)&__aeabi_dcmpgt},
    {"__aeabi_dmul", (uintptr_t)&__aeabi_dmul},
    {"__aeabi_f2d", (uintptr_t)&__aeabi_f2d},
    {"__aeabi_fadd", (uintptr_t)&__aeabi_fadd},
    {"__aeabi_fsub", (uintptr_t)&__aeabi_fsub},
    {"__aeabi_idiv", (uintptr_t)&__aeabi_idiv},
    {"__aeabi_idivmod", (uintptr_t)&__aeabi_idivmod},
    {"__aeabi_l2d", (uintptr_t)&__aeabi_l2d},
    {"__aeabi_l2f", (uintptr_t)&__aeabi_l2f},
    {"__aeabi_ldivmod", (uintptr_t)&__aeabi_ldivmod},
    {"__aeabi_uidiv", (uintptr_t)&__aeabi_uidiv},
    {"__aeabi_uidivmod", (uintptr_t)&__aeabi_uidivmod},
    {"__aeabi_uldivmod", (uintptr_t)&__aeabi_uldivmod},
    {"__android_log_print", (uintptr_t)&__android_log_print},
    {"__assert2", (uintptr_t)&__assert2},
    {"__cxa_atexit", (uintptr_t)&__cxa_atexit},
    {"__cxa_finalize", (uintptr_t)&__cxa_finalize},
    {"__errno", (uintptr_t)&__errno},
    {"__sF", (uintptr_t)&__sF_fake},
    {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail},
    {"__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake},
    {"abort", (uintptr_t)&abort},
    {"atanf", (uintptr_t)&atan},
    {"atan2f", (uintptr_t)&atan2f},
    {"atoi", (uintptr_t)&atoi},
    {"calloc", (uintptr_t)&calloc},
    {"cos", (uintptr_t)&cos},
    {"cosf", (uintptr_t)&cosf},
    {"dlopen", (uintptr_t)&ret1},
    {"dlsym", (uintptr_t)&ret1},
    {"dlclose", (uintptr_t)&ret1},
    {"fclose", (uintptr_t)&fclose},
    {"floorf", (uintptr_t)&floorf},
    {"fopen", (uintptr_t)&fopen},
    {"fprintf", (uintptr_t)&fprintf},
    {"fread", (uintptr_t)&fread},
    {"free", (uintptr_t)&free},
    {"fseek", (uintptr_t)&fseek},
    {"ftell", (uintptr_t)&ftell},
    {"fwrite", (uintptr_t)&fwrite},
    {"gettimeofday", (uintptr_t)&gettimeofday},
    {"glAlphaFunc", (uintptr_t)&glAlphaFunc},
    {"glBindTexture", (uintptr_t)&glBindTexture},
    {"glBlendFunc", (uintptr_t)&glBlendFunc},
    {"glClear", (uintptr_t)&glClear},
    {"glClearColor", (uintptr_t)&glClearColor},
    {"glColor4ub", (uintptr_t)&glColor4ub},
    {"glColorPointer", (uintptr_t)&glColorPointer},
    {"glCullFace", (uintptr_t)&glCullFace},
    {"glDeleteTextures", (uintptr_t)&glDeleteTextures},
    {"glDepthFunc", (uintptr_t)&glDepthFunc},
    {"glDepthMask", (uintptr_t)&glDepthMask},
    {"glDisable", (uintptr_t)&glDisable},
    {"glDisableClientState", (uintptr_t)&glDisableClientState},
    {"glDrawArrays", (uintptr_t)&glDrawArrays},
    {"glEnable", (uintptr_t)&glEnable},
    {"glEnableClientState", (uintptr_t)&glEnableClientState},
    {"glGenTextures", (uintptr_t)&glGenTextures},
    {"glGetError", (uintptr_t)&glGetError},
    {"glLightfv", (uintptr_t)&glLightfv},
    {"glLoadIdentity", (uintptr_t)&glLoadIdentity},
    {"glLoadMatrixf", (uintptr_t)&glLoadMatrixf},
    {"glMatrixMode", (uintptr_t)&glMatrixMode},
    {"glMultMatrixf", (uintptr_t)&glMultMatrixf},
    {"glOrthof", (uintptr_t)&glOrthof},
    {"glPopMatrix", (uintptr_t)&glPopMatrix},
    {"glPushMatrix", (uintptr_t)&glPushMatrix},
    {"glTranslatef", (uintptr_t)&glTranslatef},
    {"glTexCoordPointer", (uintptr_t)&glTexCoordPointer},
    {"glTexImage2D", (uintptr_t)&glTexImage2D},
    {"glTexParameteri", (uintptr_t)&glTexParameteriHook},
    {"glTexSubImage2D", (uintptr_t)&glTexSubImage2D},
    {"glVertexPointer", (uintptr_t)&glVertexPointer},
    {"localtime", (uintptr_t)&localtime},
    {"lrand48", (uintptr_t)&lrand48},
    {"malloc", (uintptr_t)&malloc},
    {"memchr", (uintptr_t)&memchr},
    {"memcmp", (uintptr_t)&memcmp},
    {"memcpy", (uintptr_t)&memcpy},
    {"memmove", (uintptr_t)&memmove},
    {"memset", (uintptr_t)&memset},
    {"mmap", (uintptr_t)&mmap},
    {"munmap", (uintptr_t)&munmap},
    {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},
    {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake},
    {"pthread_cond_init", (uintptr_t)&pthread_cond_init_fake},
    {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},
    {"pthread_create", (uintptr_t)&pthread_create_fake},
    {"pthread_join", (uintptr_t)&pthread_join},
    {"pthread_key_create", (uintptr_t)&pthread_key_create},
    {"pthread_key_delete", (uintptr_t)&pthread_key_delete},
    {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake},
    {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake},
    {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake},
    {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake},
    {"pthread_mutexattr_destroy", (uintptr_t)&pthread_mutexattr_destroy},
    {"pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init},
    {"pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype},
    {"pthread_setspecific", (uintptr_t)&pthread_setspecific},
    {"pthread_getspecific", (uintptr_t)&pthread_getspecific},
    {"qsort", (uintptr_t)&qsort},
    {"raise", (uintptr_t)&raise},
    {"realloc", (uintptr_t)&realloc},
    {"sin", (uintptr_t)&sin},
    {"sinf", (uintptr_t)&sinf},
    {"slCreateEngine", (uintptr_t)&slCreateEngine},
    {"snprintf", (uintptr_t)&snprintf},
    {"srand48", (uintptr_t)&srand48},
    {"sprintf", (uintptr_t)&sprintf},
    {"sqrt", (uintptr_t)&sqrt},
    {"sqrtf", (uintptr_t)&sqrtf},
    {"strcat", (uintptr_t)&strcat},
    {"strchr", (uintptr_t)&strchr},
    {"strcmp", (uintptr_t)&strcmp},
    {"strcpy", (uintptr_t)&strcpy},
    {"strlen", (uintptr_t)&strlen},
    {"strncasecmp", (uintptr_t)&strncasecmp},
    {"strncmp", (uintptr_t)&strncmp},
    {"strncpy", (uintptr_t)&strncpy},
    {"strrchr", (uintptr_t)&strrchr},
    {"strtok", (uintptr_t)&strtok},
    {"strtod", (uintptr_t)&strtod},
    {"strtol", (uintptr_t)&strtol},
    {"tanf", (uintptr_t)&tanf},
    {"time", (uintptr_t)&time},
    {"usleep", (uintptr_t)&usleep},
    {"vsnprintf", (uintptr_t)&vsnprintf},
};

int check_kubridge(void) {
  int search_unk[2];
  return _vshKernelSearchModuleByName("kubridge", search_unk);
}

int file_exists(const char *path) {
  SceIoStat stat;
  return sceIoGetstat(path, &stat) >= 0;
}

int main(int argc, char *argv[]) {

  // Check if we want to start the companion app
  sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
  SceAppUtilAppEventParam eventParam;
  sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
  sceAppUtilReceiveAppEvent(&eventParam);
  if (eventParam.type == 0x05) {
    char buffer[2048];
    sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
    if (strstr(buffer, "-config"))
      sceAppMgrLoadExec("app0:/companion.bin", NULL, NULL);
  }

  loadOptions();

  sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
  sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,
                           SCE_TOUCH_SAMPLING_STATE_START);

  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);

  if (check_kubridge() < 0)
    fatal_error("Error kubridge.skprx is not installed.");

  if (!file_exists("ur0:/data/libshacccg.suprx") &&
      !file_exists("ur0:/data/external/libshacccg.suprx"))
    fatal_error("Error libshacccg.suprx is not installed.");

  InitJNIEnv();

  if (so_load(&ff3_mod, SO_PATH, LOAD_ADDRESS) < 0)
    fatal_error("Error could not load %s.", SO_PATH);

  so_relocate(&ff3_mod);
  so_resolve(&ff3_mod, default_dynlib, sizeof(default_dynlib), 0);
  patch_game();
  so_flush_caches(&ff3_mod);

  so_initialize(&ff3_mod);

  SceUID thid =
      sceKernelCreateThread("main_thread", (SceKernelThreadEntry)main_thread,
                            0x40, 1024 * 1024, 0, 0, NULL);
  sceKernelStartThread(thid, 0, NULL);
  return sceKernelExitDeleteThread(0);
}
