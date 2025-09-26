#pragma once

#include <cstdint>
#include <cstring>

typedef void *HWND;
typedef void *HMODULE;
typedef void *HINSTANCE;

typedef uint32_t DWORD;
typedef long LRESULT;

typedef struct tagMSG
{
  int message;
  int wParam;
  int lParam;
} MSG;

typedef struct tagACCEL
{
  uint8_t fVirt;
  uint16_t key;
  uint16_t cmd;
} ACCEL;

