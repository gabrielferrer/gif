#include <windows.h>
#include <wingdi.h>
#include <stdio.h>
#include "sys.h"
#include "defs.h"
#include "gif.h"

#define INTERLACEDSTAGES               4

typedef struct interlaced_s {
	unsigned int                       startingrow;
	unsigned char                      increment;
} interlaced_t;

const interlaced_t INTERLACEDDATA[INTERLACEDSTAGES] = {{0, 8}, {4, 8}, {2, 4}, {1, 2}};
const char* WINCLASSNAME                            = "GIFTEST";
const char* WINDOWNAME                              = "GIF TEST";

LARGE_INTEGER                          Frequency;
LARGE_INTEGER                          PerformanceCount;

HINSTANCE                              HInst;
HWND                                   Hwnd;
BITMAPINFO*                            BI;
HBITMAP                                DIBSection;
char*                                  Scene;
unsigned int                           ScreenWidth;
unsigned int                           ScreenHeight;
BYTE                                   FillSamples;

// The GIF file.

FILE*                                  GIFFile;

long GetTick () {
	if (!QueryPerformanceCounter (&PerformanceCount)) {
		SYS_Exit ("GetTick", "QueryPerformanceCounter");
	}

	// Convert to microseconds.
	PerformanceCount.QuadPart *= 1000000;
	PerformanceCount.QuadPart /= Frequency.QuadPart;

	return (long) PerformanceCount.QuadPart;
}

LRESULT CALLBACK M_WProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_DESTROY:
			PostQuitMessage (0);

		return 0;
	}

	return DefWindowProc (hwnd, uMsg, wParam, lParam);
}

unsigned char M_GetFillSamples (UNSIGNED width) {
	unsigned char s;

	s = 3 * width % 4;

	if (s > 0) {
		return (4 - s);
	} else {
		return 0;
	}
}

GBOOL M_InitGraphics (HINSTANCE hInst, unsigned int width, unsigned int height) {
	HDC      hdc;
	WNDCLASS wc;
	RECT     r;
	DWORD    wStyle;

	HInst            = hInst;
	ScreenWidth      = width;
	ScreenHeight     = height;
	FillSamples      = M_GetFillSamples (ScreenWidth);
	wc.style         = 0;
	wc.lpfnWndProc   = (void*) &M_WProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = HInst;
	wc.hIcon         = LoadIcon (NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor (NULL, IDC_CROSS);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = WINCLASSNAME;

	if (!RegisterClass (&wc)) {
		return GFALSE;
	}

	wStyle   = WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
	r.left   = 0;
	r.top    = 0;
	r.right  = ScreenWidth;
	r.bottom = ScreenHeight;

	AdjustWindowRect (&r, wStyle, GFALSE);

	if ((Hwnd = CreateWindow (WINCLASSNAME, WINDOWNAME, wStyle, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL,
			NULL, HInst, NULL)) == NULL) {
		return GFALSE;
	}

	UpdateWindow (Hwnd);
	ShowWindow (Hwnd, SW_SHOW);

	if ((hdc = GetDC (Hwnd)) == NULL) {
		return GFALSE;
	}

	if ((BI = (BITMAPINFO*) malloc (sizeof (BITMAPINFO))) == NULL) {
		return GFALSE;
	}

	BI->bmiHeader.biSize          = sizeof (BITMAPINFOHEADER);
	BI->bmiHeader.biWidth         = ScreenWidth;
	BI->bmiHeader.biHeight        = -ScreenHeight;
	BI->bmiHeader.biPlanes        = 1;
	BI->bmiHeader.biBitCount      = 24;
	BI->bmiHeader.biCompression   = BI_RGB;
	BI->bmiHeader.biSizeImage     = 0;
	BI->bmiHeader.biXPelsPerMeter = 0;
	BI->bmiHeader.biYPelsPerMeter = 0;
	BI->bmiHeader.biClrUsed       = 0;
	BI->bmiHeader.biClrImportant  = 0;
	BI->bmiColors[0]              = (RGBQUAD) {0,0,0,0};

	if ((DIBSection = CreateDIBSection (hdc, BI, DIB_RGB_COLORS, (void*) &Scene, NULL, 0)) == NULL) {
		return GFALSE;
	}

	ReleaseDC (Hwnd, hdc);

	return GTRUE;
}

void M_CleanUp () {
	UnregisterClass (WINCLASSNAME, HInst);

	if (Hwnd) {
		DestroyWindow (Hwnd);
	}

	if (DIBSection) {
		DeleteObject (DIBSection);
	}

	if (BI) {
		free (BI);
	}
}

void M_GetColor (rgb_t* ct, rgb_t* color, GBYTE index) {
	color->red   = ct[index].red;
	color->green = ct[index].green;
	color->blue  = ct[index].blue;
}

void M_DrawInterlaced (image_t* image, rgb_t* gct, GBOOL bk, BYTE bkidx, unsigned int x, unsigned int y) {
	unsigned int s, d, j, p, u, v;
	GBYTE        k;
	rgb_t        c;

	for (p = 0, s = 0; p < INTERLACEDSTAGES; p++) {
		for (d = INTERLACEDDATA[p].startingrow; d < image->height; d += INTERLACEDDATA[p].increment, s++) {
			v = (y + d) * (3 * ScreenWidth + FillSamples) + 3 * x;

			for (j = 0; j < image->width; j++) {
				u = s * image->width + j;
				k = ((GBYTE*) (image->indexes->data))[u];

				if (gct) {
					M_GetColor (gct, &c, k);
				} else {
					M_GetColor (image->lct, &c, k);
				}

				if ((bk && bkidx == k) || !image->transparent || image->trnspindex != k) {
					Scene[v]     = c.blue;
					Scene[v + 1] = c.green;
					Scene[v + 2] = c.red;
				} else {
					Scene[v]     = 0;
					Scene[v + 1] = 0;
					Scene[v + 2] = 0;
				}

				v += 3;
			}
		}
	}
}

void M_DrawNormal (image_t* image, rgb_t* gct, GBOOL bk, BYTE bkidx, unsigned int x, unsigned int y) {
	unsigned int i, j, u, v;
	GBYTE        k;
	rgb_t        c;

	for (i = 0; i < image->height; i++) {
		v = (y + i) * (3 * ScreenWidth + FillSamples) + 3 * x;

		for (j = 0; j < image->width; j++) {
			u = i * image->width + j;
			k = ((GBYTE*) (image->indexes->data))[u];

			if (gct) {
				M_GetColor (gct, &c, k);
			} else {
				M_GetColor (image->lct, &c, k);
			}

			if ((bk && bkidx == k) || !image->transparent || image->trnspindex != k) {
				Scene[v]     = c.blue;
				Scene[v + 1] = c.green;
				Scene[v + 2] = c.red;
			} else {
				Scene[v]     = 0;
				Scene[v + 1] = 0;
				Scene[v + 2] = 0;
			}

			v += 3;
		}
	}
}

void M_Draw (image_t* image, rgb_t* gct, GBOOL bk, BYTE bkidx, unsigned int x, unsigned int y) {
	if (!image) {
		return;
	}

	if (image->interlaced) {
		M_DrawInterlaced (image, gct, bk, bkidx, x, y);
	} else {
		M_DrawNormal (image, gct, bk, bkidx, x, y);
	}
}

void M_Render () {
	HDC     hdc, chdc;
	HBITMAP hbmp;

	if ((hdc = GetDC (Hwnd)) == NULL) {
		return;
	}

	if ((chdc = CreateCompatibleDC (hdc)) == NULL) {
		return;
	}

	if ((hbmp = SelectObject (chdc, DIBSection)) == NULL) {
		return;
	}

	if (!BitBlt (hdc, 0, 0, ScreenWidth, ScreenHeight, chdc, 0, 0, SRCCOPY)) {
		return;
	}

	ReleaseDC (Hwnd, hdc);
	SelectObject (chdc, hbmp);
	DeleteDC (chdc);
}

void M_BlankScreen () {
	memset(Scene, 0, (3 * ScreenWidth + FillSamples) * ScreenHeight);
}

GBOOL M_MoveStreamPointer (long offset) {
	if (fseek (GIFFile, offset, SEEK_CUR)) {
		return GFALSE;
	}

	return GTRUE;
}

GBOOL M_ReadStream (void* ptr, unsigned long count) {
	return fread(ptr, sizeof (GBYTE), count, GIFFile) == count * sizeof (GBYTE);
}

int WINAPI WinMain (HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow) {
	gif_t*       gif;
	image_t*     i;
	MSG          msg;
	GBOOL        done;
	char*        filename;
	unsigned int len;
	DWORD        tickcount;
	DWORD        prevtickcount;
	DWORD        totaltime;
	TCHAR        szFileName[MAX_PATH];

	len = strlen (lpszArgument);

	if (len == 0) {
		GetModuleFileName(NULL, szFileName, MAX_PATH);
		printf("%s", "Usage: %s \"filename\"", szFileName);
	} else {
		if ((filename = (char*) malloc (len - 2 + 1)) == NULL) {
			return -1;
		}

		strncpy (filename, lpszArgument + 1, len - 2);
		filename[len - 2] = '\0';

		if ((GIFFile = fopen (filename, "rb")) == NULL) {
			goto clean2;
		}

		if (!GIF_ProcessStream (&gif, &M_ReadStream, &M_MoveStreamPointer)) {
			goto clean;
		}

		if (!M_InitGraphics (hThisInstance, gif->screenwidth, gif->screenheight)) {
			goto clean;
		}

		done          = GFALSE;
		totaltime     = 0;
		prevtickcount = GetTick();

		M_BlankScreen ();

		while (!done) {
			if (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					done = TRUE;
				} else {
					TranslateMessage (&msg);
					DispatchMessage (&msg);
				}
			}

			if (i == NULL) {
				i = gif->images;
			}

			if (i->delaytime > 0) {
				if (totaltime > i->delaytime * 10) {
					i = i->next;

					if (i == NULL) {
						i = gif->images;
					}

					totaltime = 0;
				}

				M_Draw (i, gif->gct, gif->background, gif->bkgindex, i->left, i->top);
			} else {
				while (i) {
					M_Draw (i, gif->gct, gif->background, gif->bkgindex, i->left, i->top);
					i = i->next;
				}
			}

			M_Render ();

			tickcount = GetTick();

			// Check wrap-around.

			if (tickcount < prevtickcount) {
				totaltime += 0xFFFFFFFF - prevtickcount + tickcount;
			} else {
				totaltime += tickcount - prevtickcount;
			}

			prevtickcount = tickcount;
		}

		GIF_FreeGif (gif);
	}

	fclose (GIFFile);
	free (filename);
	M_CleanUp ();

	return 0;

clean:
	fclose (GIFFile);
clean2:
	free (filename);
	M_CleanUp ();

	return -1;
}

