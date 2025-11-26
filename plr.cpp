#include "stdafx.h"
#include "plr.h"
#include "mad.h"
#include <MMReg.h>

#pragma comment(lib, "libmad")
#pragma comment(lib, "Winmm")

#define MAX_LOADSTRING 100

HINSTANCE hInst;
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

  HWAVEOUT hWaveOut;
  WAVEFORMATEX wfx;
  MMRESULT result;

FILE *m_mp3;

struct buffer {
  unsigned char const *start;
  unsigned long length;
};

static enum mad_flow input(void *data, mad_stream *stream) {
  buffer *m_buffer = (buffer *)data;

  if (!m_buffer->length) {
    return MAD_FLOW_STOP;
  }

  mad_stream_buffer(stream, m_buffer->start, m_buffer->length);

  m_buffer->length = 0;

  return MAD_FLOW_CONTINUE;
}

static inline
signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static
enum mad_flow output(void *data,
		     struct mad_header const *header,
		     struct mad_pcm *pcm)
{
  unsigned int nchannels, nsamples;
  mad_fixed_t const *left_ch, *right_ch;

  /* pcm->samplerate contains the sampling frequency */

  nchannels = pcm->channels;
  nsamples  = pcm->length;
  left_ch   = pcm->samples[0];
  right_ch  = pcm->samples[1];

  int size = nchannels * nsamples * 2;
	char *samples = (char *)malloc( size );
  char *dst = samples;

	while (nsamples--) {
		signed int sample;

		// output sample(s) in 16-bit signed little-endian PCM
		sample = scale(*left_ch++);
		*dst++ = ((sample >> 0) & 0xff);
		*dst++ = ((sample >> 8) & 0xff);
		if (nchannels == 2) {
			sample = scale(*right_ch++);
			*dst++ = ((sample >> 0) & 0xff);
			*dst++ = ((sample >> 8) & 0xff);
		}
	}


  if(!hWaveOut) {
    memset(&wfx, 0, sizeof(WAVEFORMATEX));

    wfx.nSamplesPerSec = pcm->samplerate;
    wfx.wBitsPerSample = 16;
    wfx.nChannels = pcm->channels;
    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
  
  
    if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
      fprintf(stderr, "unable to open WAVE_MAPPER device\n");
      ExitProcess(1);
    }
  }

  WAVEHDR *wheader = new WAVEHDR;

  memset(wheader, 0, sizeof(WAVEHDR));
  wheader->dwBufferLength = size;
  wheader->lpData = (LPSTR)samples;
      
  waveOutPrepareHeader(hWaveOut, wheader, sizeof(WAVEHDR));

  while(waveOutWrite(hWaveOut, wheader, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) Sleep(10);

  return MAD_FLOW_CONTINUE;
}

static enum mad_flow error(void *data, struct mad_stream *stream, struct mad_frame *frame) {
  buffer *m_buffer = (buffer *)data;

  fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - m_buffer->start);

  return MAD_FLOW_CONTINUE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
  DWORD m_fread = 0;

  //m_mp3 = CreateFile(L"...", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
  //unsigned long m_end = SetFilePointer(m_mp3, 0, 0, 2);
  //SetFilePointer(m_mp3, 0, 0, 0);

  m_mp3 = fopen(".....file...name...here...", "rb");
  fseek(m_mp3, 0, SEEK_END);
  unsigned int m_len = ftell(m_mp3);
  fseek(m_mp3, 0, SEEK_SET);

  struct buffer buffer;
  struct mad_decoder decoder;
  int result;

  buffer.start  = (unsigned char *)malloc(m_len);
  buffer.length = m_len;

  fread((void *)buffer.start, 1, m_len, m_mp3);

  //ReadFile(m_mp3, (void *)buffer.start, m_len, &m_fread, NULL);

  /* configure input, output, and error functions */

  mad_decoder_init(&decoder, &buffer,
		   input, 0 /* header */, 0 /* filter */, output,
		   error, 0 /* message */);

  /* start decoding */

  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  /* release the decoder */

  mad_decoder_finish(&decoder);
  
	MSG msg;
	HACCEL hAccelTable;

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_PLR, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow)) {
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PLR));

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PLR));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_PLR);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 300, 100, NULL, NULL, hInstance, NULL);

   if (!hWnd) {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message) {
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId) {
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
