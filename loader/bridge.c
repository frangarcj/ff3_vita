#include "bridge.h"
#include <limits.h>
#include <math.h>
#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/system_param.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image.h"
#include "stb_truetype.h"

#include "zlib.h"
#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#include <vitaGL.h>

#include "config.h"
#include "dialog.h"

#define SAVE_FILENAME "ux0:/data/ff3"
#define OBB_FILE "ux0:/data/ff3/main.obb"
#define FONT_FILE "app0:/NotoSansJP-Regular.ttf"

unsigned char *header = NULL;
int header_length = 0;

void decodeArray(unsigned char *bArr, int size, uint key) {
  for (int n = 0; n < size; n++) {
    key = key * 0x41c64e6d + 0x3039;
    bArr[n] = bArr[n] ^ (unsigned char)(key >> 0x18);
  }
}

static int getInt(unsigned char *bArr, int i) {
  return *(unsigned int *)(&bArr[i]);
}

unsigned char *gzipRead(unsigned char *bArr, int *bArr_length) {
  unsigned int readInt = __builtin_bswap32(getInt(bArr, 0));
  unsigned char *bArr2 = calloc(readInt, sizeof(unsigned char));
  unsigned char *bArr3 = &bArr[4];

  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  // setup "b" as the input and "c" as the compressed output
  infstream.avail_in = *bArr_length - 4; // size of input
  infstream.next_in = bArr3;             // input char array
  infstream.avail_out = readInt;         // size of output
  infstream.next_out = bArr2;            // output char array

  // the actual DE-compression work.
  inflateInit2(&infstream, MAX_WBITS | 16);
  inflate(&infstream, Z_FULL_FLUSH);
  inflateEnd(&infstream);

  *bArr_length = readInt;
  return bArr2;
}

unsigned char *m476a(char *str, int *file_length) {
  int i;

  unsigned char *bArr = header;
  if (bArr != NULL) {
    int a = getInt(bArr, 0);
    int i2 = 0;
    i = 0;
    while (a > i2) {
      int i3 = (i2 + a) / 2;
      int i4 = i3 * 12;
      int a2 = getInt(header, i4 + 4);
      int i5 = 0;
      for (int i6 = 0; i6 < strlen(str) && i5 == 0; i6++) {
        i5 = (header[a2 + i6] & 0xFF) - (str[i6] & 0xFF);
      }
      if (i5 == 0) {
        i5 = header[a2 + strlen(str)] & 0xFF;
      }
      if (i5 == 0) {
        i = i4 + 8;
        a = i3;
        i2 = a;
      } else if (i5 > 0) {
        a = i3;
      } else {
        i2 = i3 + 1;
      }
    }
  } else {
    i = 0;
  }
  if (i == 0) {
    return NULL;
  }

  const char *a = OBB_FILE;
  FILE *fp = fopen(a, "r");
  int a3 = getInt(header, i + 0);
  fseek(fp, a3, SEEK_SET);

  *file_length = getInt(header, i + 4);
  unsigned char *bArr2 = malloc(*file_length);

  for (int i7 = 0; i7 < *file_length;
       i7 += fread(&bArr2[i7], sizeof(unsigned char), *file_length - i7, fp)) {
  }

  fclose(fp);

  decodeArray(bArr2, *file_length, a3 + 84861466u);

  unsigned char *a4 = gzipRead(bArr2, file_length);

  free(bArr2);

  return a4;
}

int readHeader() {
  const char *a = OBB_FILE;
  FILE *fp = fopen(a, "r");
  fseek(fp, 0L, SEEK_END);
  int length = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  unsigned char bArr[16];

  for (int i = 0; i < 16;
       i += fread(&bArr[i], sizeof(unsigned char), 16 - i, fp)) {
  }

  decodeArray(bArr, 16, 84861466u);

  if (getInt(bArr, 0) != 826495553) {
    printf("initFileTable: Header Error\n");
    return 0;
  } else if (length != getInt(bArr, 4)) {
    printf("initFileTable: Size Error\n");
    return 0;
  } else {
    unsigned int a2 = getInt(bArr, 8);
    header_length = getInt(bArr, 12);
    header = malloc(header_length);

    fseek(fp, (long)(a2 - 16), SEEK_CUR);

    for (int i = 0; i < header_length;
         i += fread(&header[i], sizeof(unsigned char), header_length - i, fp)) {
    }

    decodeArray(header, header_length, a2 + 84861466u);

    unsigned char *header2 = gzipRead(header, &header_length);

    free(header);

    header = header2;

    fclose(fp);
  }
  return 1;
}

void toUtf8(const char *src, size_t length, char *dst, const char *src_encoding,
            size_t *dst_length_p) {

  UErrorCode status = U_ZERO_ERROR;
  UConverter *conv;
  int32_t len;

  uint16_t *temp = malloc(length * 3);
  u_setDataDirectory("app0:/");
  conv = ucnv_open(src_encoding, &status);

  len = ucnv_toUChars(conv, temp, length * 3, src, length, &status);
  ucnv_close(conv);

  conv = ucnv_open("utf-8", &status);
  *dst_length_p = ucnv_fromUChars(conv, dst, length * 3, temp, len, &status);
  ucnv_close(conv);

  free(temp);
}

unsigned char *decodeString(unsigned char *bArr, int *bArr_length) {

  int lang = getCurrentLanguage();
  if (lang >= 6) {
    return bArr;
  }

  unsigned char *utf8_strings = malloc(*bArr_length * 3);

  int entry_number = *(int *)&bArr[8];
  int header_length = (entry_number * 12) + 16;

  memcpy(utf8_strings, bArr, header_length);

  int utf8_offset = header_length;
  int entry_index = 0;
  int entry_offset = 16;

  while (entry_index < entry_number) {
    *(int *)&utf8_strings[entry_offset + 8] = utf8_offset;

    unsigned char string_number = bArr[entry_offset + 4];
    int string_offset = *(int *)&bArr[entry_offset + 8];
    for (int string_index = 0; string_index < string_number; string_index++) {
      int string_length = 0;
      while (bArr[string_offset + string_length] != 0) {
        string_length++;
      }
      unsigned int utf8_length = 0;
      toUtf8((const char *)&bArr[string_offset], string_length,
             (char *)&utf8_strings[utf8_offset],
             lang == 0 ? "shift_jis" : "windows-1252", &utf8_length);

      utf8_strings[utf8_offset + utf8_length] = 0;

      string_offset += string_length + 1;
      utf8_offset += utf8_length + 1;
    }
    utf8_offset = utf8_offset + 1;
    utf8_strings[utf8_offset] = 0;
    entry_offset += 12;
    entry_index++;
  }

  unsigned char *final_strings = malloc(utf8_offset);
  memcpy(final_strings, utf8_strings, utf8_offset);
  free(utf8_strings);
  *bArr_length = utf8_offset;
  return final_strings;
}

jni_bytearray *loadFile(char *str) {
  char *lang[] = {"ja", "en",    "fr",    "de", "it",
                  "es", "zh_CN", "zh_TW", "ko", "th"};

  char *substring = strrchr(str, 46);

  substring = substring == NULL ? str : substring;
  char temp_path[512];
  int file_length;
  sprintf(temp_path, "%s.lproj/%s", lang[getCurrentLanguage()], str);
  unsigned char *a = m476a(temp_path, &file_length);
  if (a == NULL) {
    sprintf(temp_path, "files/%s", str);
    a = m476a(temp_path, &file_length);
  }
  if (a == NULL) {
    return NULL;
  }

  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = a;
  result->size = file_length;

  if (strcmp(substring, ".msd") || str[0] == 'e')
    return result;
  if (str[0] == 'n' && str[1] == 'o' && str[2] == 'a')
    return result;

  unsigned char *b = decodeString(a, &file_length);

  if (b != a)
    free(a);

  result->elements = b;
  result->size = file_length;

  return result;
}

jni_bytearray *loadRawFile(char *str) {

  int file_length;
  unsigned char *a = m476a(str, &file_length);

  if (a == NULL) {
    return NULL;
  }

  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = a;
  result->size = file_length;

  return result;
}

jni_bytearray *getSaveFileName() {

  char *buffer = SAVE_FILENAME;
  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = malloc(strlen(buffer) + 1);
  // Sets the value
  strcpy((char *)result->elements, buffer);
  result->size = strlen(buffer) + 1;

  return result;
}

void createSaveFile(size_t size) {
  char *buffer = malloc(size);
  FILE *fd = fopen(SAVE_FILENAME "/save.bin", "wb");
  fwrite(buffer, sizeof(char), size, fd);
  fclose(fd);
  free(buffer);
}

uint64_t j3 = 0;

uint64_t getCurrentFrame(uint64_t j) {

  while (1) {
    uint64_t j2 = sceKernelGetProcessTimeWide() * 3 / 100000;
    if (j2 != j3) {
      j3 = j2;
      return j2;
    }
    sceKernelDelayThread(1000);
  }
}

#define RGBA8(r, g, b, a)                                                      \
  ((((a)&0xFF) << 24) | (((b)&0xFF) << 16) | (((g)&0xFF) << 8) |               \
   (((r)&0xFF) << 0))

jni_intarray *loadTexture(jni_bytearray *bArr) {

  jni_intarray *texture = malloc(sizeof(jni_intarray));

  int x, y, channels_in_file;
  unsigned char *temp = stbi_load_from_memory(bArr->elements, bArr->size, &x,
                                              &y, &channels_in_file, 4);

  texture->size = x * y + 2;
  texture->elements = malloc(texture->size * sizeof(int));
  texture->elements[0] = x;
  texture->elements[1] = y;

  for (int n = 0; n < y; n++) {
    for (int m = 0; m < x; m++) {
      unsigned char *color = (unsigned char *)&(((uint32_t *)temp)[n * x + m]);
      texture->elements[2 + n * x + m] =
          RGBA8(color[2], color[1], color[0], color[3]);
    }
  }

  free(temp);

  return texture;
}

int isDeviceAndroidTV() { return 1; }

stbtt_fontinfo *info = NULL;
unsigned char *fontBuffer = NULL;

void initFont() {

  long size;

  if (info != NULL)
    return;

  FILE *fontFile = fopen(FONT_FILE, "rb");
  fseek(fontFile, 0, SEEK_END);
  size = ftell(fontFile);       /* how long is the file ? */
  fseek(fontFile, 0, SEEK_SET); /* reset */

  fontBuffer = malloc(size);

  fread(fontBuffer, size, 1, fontFile);
  fclose(fontFile);

  info = malloc(sizeof(stbtt_fontinfo));

  /* prepare font */
  if (!stbtt_InitFont(info, fontBuffer, 0)) {
    printf("failed\n");
  }
}

static inline uint32_t utf8_decode_unsafe_2(const char *data) {
  uint32_t codepoint;
  codepoint = ((data[0] & 0x1F) << 6);
  codepoint |= (data[1] & 0x3F);
  return codepoint;
}

static inline uint32_t utf8_decode_unsafe_3(const char *data) {
  uint32_t codepoint;
  codepoint = ((data[0] & 0x0F) << 12);
  codepoint |= (data[1] & 0x3F) << 6;
  codepoint |= (data[2] & 0x3F);
  return codepoint;
}

static inline uint32_t utf8_decode_unsafe_4(const char *data) {
  uint32_t codepoint;
  codepoint = ((data[0] & 0x07) << 18);
  codepoint |= (data[1] & 0x3F) << 12;
  codepoint |= (data[2] & 0x3F) << 6;
  codepoint |= (data[3] & 0x3F);
  return codepoint;
}

jni_intarray *drawFont(char *word, int size, int i2, int i3) {
  initFont();

  jni_intarray *texture = malloc(sizeof(jni_intarray));
  texture->size = size * size + 5;
  texture->elements = malloc(texture->size * sizeof(int));

  int b_w = size; /* bitmap width */
  int b_h = size; /* bitmap height */

  /* calculate font scaling */
  float scale = stbtt_ScaleForPixelHeight(info, size);

  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);

  int i = 0;
  while (word[i]) {
    i++;
    if (i == 4)
      break;
  }

  int codepoint;
  switch (i) {
  case 0: // This should never happen
    codepoint = 32;
    break;
  case 2:
    codepoint = utf8_decode_unsafe_2(word);
    break;
  case 3:
    codepoint = utf8_decode_unsafe_3(word);
    break;
  case 4:
    codepoint = utf8_decode_unsafe_4(word);
    break;
  default:
    codepoint = word[0];
    break;
  }

  int ax;
  int lsb;
  stbtt_GetCodepointHMetrics(info, codepoint, &ax, &lsb);

  if (codepoint == 32) {
    texture->elements[0] = roundf(ax * scale);
    return texture;
  }

  /* create a bitmap for the phrase */
  unsigned char *bitmap = calloc(b_w * b_h, sizeof(unsigned char));

  /* get bounding box for character (may be offset to account for chars that dip
   * above or below the line) */
  int c_x1, c_y1, c_x2, c_y2;
  stbtt_GetCodepointBitmapBox(info, codepoint, scale, scale, &c_x1, &c_y1,
                              &c_x2, &c_y2);

  /* compute y (different characters have different heights) */
  int y = roundf(ascent * scale) + c_y1;

  /* render character (stride and offset is important here) */
  int byteOffset = roundf(lsb * scale) + (y * b_w);

  stbtt_MakeCodepointBitmap(info, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1,
                            b_w, scale, scale, codepoint);

  texture->elements[0] = (c_x2 - c_x1 + roundf(lsb * scale));
  texture->elements[1] = 0;
  texture->elements[2] = 0;

  for (int n = 0; n < size * size; n++) {
    texture->elements[5 + n] =
        RGBA8(bitmap[n], bitmap[n], bitmap[n], bitmap[n]);
  }

  free(bitmap);
  return texture;
}

int editText = -1;

void createEditText(char *str) { editText = init_ime_dialog("", str); }

char *getEditText() {
  char *result = NULL;
  while (!result && editText != -1) {
    result = get_ime_dialog_result();
    vglSwapBuffers(GL_TRUE);
  }
  editText = -1;
  return result;
}

int getCurrentLanguage() {

  if (options.lang)
    return (options.lang - 1);

  int lang = -1;
  sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &lang);
  switch (lang) {
  case SCE_SYSTEM_PARAM_LANG_JAPANESE:
    return 0;
  case SCE_SYSTEM_PARAM_LANG_FRENCH:
    return 2;
  case SCE_SYSTEM_PARAM_LANG_GERMAN:
    return 3;
  case SCE_SYSTEM_PARAM_LANG_ITALIAN:
    return 4;
  case SCE_SYSTEM_PARAM_LANG_SPANISH:
    return 5;
  default:
    return 1;
  }
}

void loadCompanionApp() {
  sceAppMgrLoadExec("app0:/companion.bin", NULL, NULL);
}
