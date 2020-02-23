/*
 *  Copyright (c) 2013 - 2018 Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * File:   glxwindow.h
 * Author: Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *
 * Created on 7 апреля 2018 г., 11:36
 */

#ifndef GLXWINDOW_H
#define GLXWINDOW_H


#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <GL/glext.h>


inline static void 
print_gl_error(void) 
{ 
	GLenum errCode; 
	
	errCode = glGetError();
	if(errCode != GL_NO_ERROR) 
		fprintf(stderr, "OpenGl error - %s\n", gluErrorString(errCode) ); 
}


class cGLXWindow
{
public:
	
	struct	sWindowState
	{
		uint32_t uWidth; 
		uint32_t uHeight;
		int32_t	iMouseRootX;
		int32_t	iMouseRootY;
		int32_t	iMouseWinX;
		int32_t	iMouseWinY;
	};
	
	struct sWinGLXParam
	{
		uint32_t uWidth;
		uint32_t uHeight;
		int32_t iMajorGLVer;
		int32_t iMinorGLVer;
		void(*callback_redraw)(sWindowState *ws, uint32_t stateFlags);
		void(*callback_event)(XEvent* event);
	};
	
	uint32_t	static const	VIEWPORT_INIT_FLAG = 1<<1;
	uint32_t	static const	VIEWPORT_RESIZE_FLAG = 1<<2;
	uint32_t	static const	VIEWPORT_DESTROY_FLAG = 1<<3;
	
				cGLXWindow();
	virtual		~cGLXWindow();
	int32_t		create_window(sWinGLXParam *param, const char *szWinCaption);
	int32_t		update_window(void);
	void		destroy_window();
	int32_t		set_window_size(uint32_t uWidth, uint32_t uHeight);
	int32_t		set_window_fullscreen_popup();
	int32_t		unset_window_fullscreen_popup(uint32_t uWidth, uint32_t uHeight);
	void		hide_cursor(void);
	void		show_cursor(void);
	Window		get_window_XID(void);
	Display*	get_window_display();
	int32_t		get_glx_extensions_string(const char **szExtList);
	int32_t		get_gl_extensions_string(const char **szExtList);
	int32_t		check_gl_extension(const char *szExtName);
	int32_t		check_glx_extension(const char *szExtName);
	
private:
	
	int32_t		get_screen_resolution(uint32_t *puWidth, uint32_t *puHeight);
	int32_t		set_fullscreen(void);
	
	/* Callback for class returns width, height, init and resize flag */
	void	(*redraw_cb)(sWindowState *ws, uint32_t stateFlags);
	void	(*events_cb)(XEvent* event);
	
	Display					*display;
	Window					window;
	XEvent					event;
	int32_t					screen;
    uint32_t				uMask;
	
	XVisualInfo				*vi;
	Colormap				cmap;
	XSetWindowAttributes	swa;
	GLXContext				glc;
	XWindowAttributes		gwa;
	Atom					wmDelete;
	
	sWindowState			ws;
	Window					returnedWindow;
	
	PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribs;
	
};


cGLXWindow::
cGLXWindow()
{
	display = NULL;
	window = 0;
	screen = 0;
	cmap = 0;
	vi = NULL;
	glc = NULL;
	wmDelete = 0;
	
	memset(&ws, 0, sizeof(ws));
	
	redraw_cb = NULL;
	events_cb = NULL;
	
	glXCreateContextAttribs = NULL;
}


cGLXWindow::
~cGLXWindow()
{
//	destroy_window();
}


/*
 * Deinitialize window
 */
void cGLXWindow::
destroy_window()
{
	if(glc)
	{
		redraw_cb(&ws, VIEWPORT_DESTROY_FLAG);
		
		if(!glXMakeCurrent(display, None, NULL))
		{
			fprintf(stderr, "Could not release drawing context\n\r");
		}
		
		glXDestroyContext(display, glc);
		glc = NULL;
	}
   	
	if(display != NULL && window != 0)
	{
		XDestroyWindow(display, window);
		XCloseDisplay(display);
		display = NULL;
		window = 0;
	}
}


/*
 *	Create window with uWidth and uHeight dimentions
 *	from sWinGLXParam struct
 *  If uWidth and uHeight equal zero then window
 *  dimentions sets to fullscreen
 *  Returns 1 on success or -1 on fail
 */
int32_t cGLXWindow::
create_window(sWinGLXParam *param, const char *szWinCaption)
{
	Window			rootWindow = 0;
	int32_t			iCountFB;
	int32_t			iMajorVer;
	int32_t			iMinorVer;
	GLXFBConfig*	fbc = NULL;
	int32_t			iBestFBIndex = -1;
	int32_t			iBestSamplesNum = -1;
	int32_t			iSampBuf;
	int32_t			iSamplesNum;
	int32_t			i;
	GLXFBConfig		bestFBConf;
	
	if(!param)
	{
		fprintf(stderr, "Invalid input\n");
		return -1;
	}
	
	GLint classicAttr[] = 
	{
		GLX_RGBA, 
		GLX_DOUBLEBUFFER,	True, 
		GLX_DEPTH_SIZE,		24, 
		None									//zero indicates the end of the array
	};
	GLint baseAttr[] = 
	{
		GLX_X_RENDERABLE    , True,
		GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
		GLX_RENDER_TYPE     , GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
		GLX_RED_SIZE        , 8,
		GLX_GREEN_SIZE      , 8,
		GLX_BLUE_SIZE       , 8,
		GLX_ALPHA_SIZE      , 8,
		GLX_DEPTH_SIZE      , 24,
		GLX_STENCIL_SIZE    , 8,
//		GLX_SAMPLE_BUFFERS_ARB  , True,         // <-- MSAA
//		GLX_SAMPLES_ARB         , 4,            // <-- MSAA
		None									//zero indicates the end of the array
	};
	
	GLint modernAttr[10] = {0};
	i = 0;
	modernAttr[++i] = GLX_CONTEXT_MAJOR_VERSION_ARB; modernAttr[++i] = param->iMajorGLVer;
	modernAttr[++i] = GLX_CONTEXT_MINOR_VERSION_ARB; modernAttr[++i] = param->iMinorGLVer; 
	
	
	if(param->callback_redraw == NULL)
	{
		fprintf(stderr, "Invalid redraw callback function\n");
		return -1;
	}
	
	redraw_cb = (void (*)(sWindowState*, uint32_t))param->callback_redraw;
	
	if(NULL != param->callback_event)
	{
		events_cb = (void (*)(XEvent*))param->callback_event;
	}
	
	if(param->uWidth == 0 || param->uHeight == 0)
	{
		if (1 > get_screen_resolution(&param->uWidth, &param->uHeight) )
		{
			fprintf(stderr, "Could not get screen resolution\n");
			return -1;
		}
	}
	
	if(display != NULL || window != 0)
	{
		XDestroyWindow(display, window);
		XCloseDisplay(display);
		display = NULL;
		window = 0;
	}
	
	ws.uWidth = param->uWidth;
	ws.uHeight = param->uHeight;
	
/****************************** Begin init  ***********************************/	
	display = XOpenDisplay( NULL );
	if ( display == NULL )
	{
		fprintf(stderr, "Cannot open display\n");
		return -1;
	}

	screen = DefaultScreen( display );
	
	rootWindow = DefaultRootWindow(display);
	if ( rootWindow == 0 )
	{
		fprintf(stderr, "Cannot get root window XID\n");
		goto ERRORS;
	}
	
	/* FBConfigs were added in GLX version 1.3. */
	if ( !glXQueryVersion( display, &iMajorVer, &iMinorVer ) )
	{
		fprintf(stderr, "glXQueryVersion error\n");
		goto ERRORS;
	} 

	/* Invalid GL version */
	if( iMajorVer == 0 )
	{
		fprintf(stderr, "Invalid GLX version\n");
		goto ERRORS;
	}
	/* OpenGL version < 1.3, old style context initializing */
	else if(( iMajorVer == 1 ) && ( iMinorVer < 3 ))
	{
		fprintf(stderr, "OpenGL version %d.%d < 1.3, old style context initializing\n", iMajorVer, iMinorVer);
		
		vi = glXChooseVisual(display, 0, classicAttr);
		if( vi == NULL )
		{
			fprintf(stderr, "No appropriate visual found\n");
			goto ERRORS;
		}
		
		cmap = XCreateColormap(display, rootWindow, vi->visual, AllocNone);
		if( cmap == 0 )
		{
			fprintf(stderr, "Cannot create colormap\n");
			goto ERRORS;
		}

		swa.colormap = cmap;
		swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask;

		window = XCreateWindow
		(
			display,
			rootWindow,
			0,
			0,
			param->uWidth,
			param->uHeight,
			0,
			vi->depth,
			InputOutput,
			vi->visual,
			CWBackPixmap | CWBorderPixel | CWColormap | CWEventMask,
			&swa
		);

		if(window == 0)
		{
			fprintf(stderr, "Cannot create window\n");
			goto ERRORS;
		}

		wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", True);
		XSetWMProtocols(display, window, &wmDelete, 1);

		if(szWinCaption != NULL)
			XStoreName(display, window, szWinCaption);

		XMapWindow( display, window );
		
		glc = glXCreateContext(display, vi, NULL, GL_TRUE);
		if(glc == NULL)
		{
			fprintf(stderr, "Cannot create OpenGL context\n");
			goto ERRORS;
		}

	}
	/* FBconfig supported by video subsystem */
	else
	{
		fbc = glXChooseFBConfig(display, DefaultScreen(display), baseAttr, &iCountFB);
		if (!fbc)
		{
			fprintf(stderr, "Failed to retrieve a framebuffer config\n" );
			goto ERRORS;
		}
		
		/* Chosen best visual ID from frame buffer object */	
		for (i=0; i<iCountFB; ++i)
		{
			glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLE_BUFFERS, &iSampBuf );
			glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLES, &iSamplesNum  );

			if ( iBestFBIndex < 0 || ((iSampBuf > 0) && (iSamplesNum > iBestSamplesNum) ) )
			{
				iBestFBIndex = i;
				iBestSamplesNum = iSamplesNum;
			}
		}

		bestFBConf = fbc[iBestFBIndex];

		vi = glXGetVisualFromFBConfig( display, bestFBConf );
		if( vi == NULL )
		{
			fprintf(stderr, "Getting visual ID from FBConfig error\n");
			goto ERRORS;
		}
		
//		fprintf( stderr, "Chosen visual ID = 0x%lu, with samples number = %d\n\r", 
//				vi->visualid,  iBestSamplesNum);
		
		cmap = XCreateColormap(display, rootWindow, vi->visual, AllocNone);
		if( cmap == 0 )
		{
			fprintf(stderr, "Cannot create colormap\n");
			goto ERRORS;
		}

		swa.colormap = cmap;
		swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask;

		window = XCreateWindow(
			display,
			rootWindow,
			0,
			0,
			param->uWidth,
			param->uHeight,
			0,
			vi->depth,
			InputOutput,
			vi->visual,
			CWBackPixmap | CWBorderPixel | CWColormap | CWEventMask,
			&swa);

		if(window == 0)
		{
			fprintf(stderr, "Cannot create window\n");
			goto ERRORS;
		}

		//XSetStandardProperties(display, window, szWinCaption,
		//				        szWinCaption, None, NULL, 0, NULL);
		//XMapRaised(display, window);

		wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", True);
		XSetWMProtocols(display, window, &wmDelete, 1);

		if(szWinCaption != NULL)
			XStoreName(display, window, szWinCaption);

		XMapWindow( display, window );
		
		glXCreateContextAttribs = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((GLubyte*)"glXCreateContextAttribsARB");

		if (glXCreateContextAttribs == NULL)
		{
			fprintf(stderr, "glXCreateContextAttribs is not supported\n");
			goto ERRORS;
		}

		glc = glXCreateContextAttribs( display, bestFBConf, 0, True, modernAttr );
		if(glc == NULL)
		{
			fprintf(stderr, "Cannot create OpenGL context\n");
			goto ERRORS;
		}

	}
	
	XSync( display, False );

    glXMakeCurrent(display, window, glc);
	
	if (!glXIsDirect(display, glc)) 
        fprintf(stderr, "Direct Rendering is not supported\n\r");
	
//	XFlush(display);
	
//	XGrabKeyboard(display, window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
//  XGrabPointer(display, window, True, ButtonPressMask, 
//	GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
	
	if(fbc)
		XFree( fbc );
	
	/* Set all flags in first running redraw */
	redraw_cb(&ws, VIEWPORT_INIT_FLAG | VIEWPORT_RESIZE_FLAG);
	glXSwapBuffers(display, window);
	
	return 1;
	
/**************************** Errors processing	*******************************/	
ERRORS:
	
	if(glc)
	{
		if(!glXMakeCurrent(display, None, NULL))
		{
			fprintf(stderr, "Could not release drawing context\n\r");
		}
		
		glXDestroyContext(display, glc);
		glc = NULL;
	}
   	
	if(display != NULL && window != 0)
	{
		XDestroyWindow(display, window);
		XCloseDisplay(display);
		display = NULL;
		window = 0;
	}
	
	return -1;
}


int32_t cGLXWindow::
check_gl_extension(const char* szExtName)
{
	if(szExtName == NULL || glc == NULL)
		return -1;

	uint32_t	i;
	const char	*szExtList = NULL;
	size_t		uExtListStrLen;
	uint32_t	uExtNameLen;
	char		szRecvExtName[64];
	uint32_t	uStartPos = 0;

	
	szExtList = (const char*)glGetString(GL_EXTENSIONS);
	if(szExtList == NULL)
		return -1;
	
	uExtListStrLen = strlen(szExtList);
	if(uExtListStrLen == 0)
		return -1;
	
	for(i=0; i<uExtListStrLen; i++)
	{
		if(szExtList[i] == ' ' || szExtList[i] == '\0')
		{
			uExtNameLen = i-uStartPos;
			/* Buffer overrun protection */
			if( uExtNameLen >= sizeof(szRecvExtName))
				uExtNameLen = sizeof(szRecvExtName)-1;

			memcpy(szRecvExtName, &szExtList[uStartPos], uExtNameLen);
			/* Add zero to end of the string */
			szRecvExtName[uExtNameLen] = 0;

			if(0 == strcmp(szRecvExtName, szExtName))
				return 1;
			
			uStartPos = i+1;
		}
	}

	return 0;
}

int32_t cGLXWindow::
get_gl_extensions_string(const char** szExtList)
{
	if(szExtList == NULL || glc == NULL )
		return -1;
	
	*szExtList = NULL;
	*szExtList = (const char*)glGetString(GL_EXTENSIONS);
	
	if(*szExtList == NULL)
		return -1;
	
	return 1;
}

int32_t cGLXWindow::
check_glx_extension(const char* szExtName)
{
	if(szExtName == NULL || display == NULL)
		return -1;

	uint32_t	i;
	const char	*szExtList = NULL;
	size_t		uExtListStrLen;
	uint32_t	uExtNameLen;
	char		szRecvExtName[64];
	uint32_t	uStartPos = 0;

	szExtList = (const char*)glXQueryExtensionsString(display, screen);
	if(szExtList == NULL)
		return -1;
	
	uExtListStrLen = strlen(szExtList);
	if(uExtListStrLen == 0)
		return -1;
	
	for(i=0; i<uExtListStrLen; i++)
	{
		if(szExtList[i] == ' ' || szExtList[i] == '\0')
		{
			uExtNameLen = i-uStartPos;
			/* Buffer overrun protection */
			if( uExtNameLen >= sizeof(szRecvExtName))
				uExtNameLen = sizeof(szRecvExtName)-1;

			memcpy(szRecvExtName, &szExtList[uStartPos], uExtNameLen);
			/* Add zero to end of the string */
			szRecvExtName[uExtNameLen] = 0;

			if(0 == strcmp(szRecvExtName, szExtName))
				return 1;
			
			uStartPos = i+1;
		}
	}

	return 0;
}


int32_t cGLXWindow::
get_glx_extensions_string(const char** szExtList)
{
	if(szExtList == NULL || display == NULL)
		return -1;
	
	*szExtList = NULL;
	*szExtList = glXQueryExtensionsString(display, screen);
	
	if(*szExtList == NULL)
		return -1;
	
	return 1;
}


/*
 * Get current screen resolution
 * Returns 1 on success or -1 on fail
 */
int32_t cGLXWindow::
get_screen_resolution(uint32_t* puWidth, uint32_t* puHeight)
{
	if(puWidth == NULL || puHeight == NULL)
	{
		fprintf(stderr, "Invalid input\n");
		return -1;
	}
	Display	*display = NULL;
	Screen	*screen = NULL;
	display = XOpenDisplay(NULL);
	screen = DefaultScreenOfDisplay(display);
	
	if(screen == NULL)
		return -1;
	
	*puHeight = screen->height;
	*puWidth  = screen->width;
	
	XCloseDisplay(display);
	
	return 1;
}


void cGLXWindow::
hide_cursor()
{
	if(display == NULL || window == 0)
		return;
	
	XFixesHideCursor(display, window);
	XFlush(display);
}


void cGLXWindow::
show_cursor()
{
	if(display == NULL || window == 0)
		return;
	
	XFixesShowCursor(display, window);
	XFlush(display);
}


/*
 *  Set window to fullscreen popup mode without borders
 *  Returns 1 on success or -1 on fail
 */
int32_t cGLXWindow::
set_window_fullscreen_popup()
{
	if(!glc)
		return -1;
	
	XEvent		e;
	uint32_t	uWidth;
	uint32_t	uHeight;
   
	Atom		NET_WM_STATE = XInternAtom(display, "_NET_WM_STATE",	0);
	Atom		NET_WM_FULLSCREEN_MONITORS = XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", 0);
	Atom		NET_WM_STATE_FULLSCREEN = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);
	
	if (1 > get_screen_resolution(&uWidth, &uHeight) )
	{
		fprintf(stderr, "Could not get screen resolution\n");
		return -1;
	}
								 
	
	e.xany.type = ClientMessage;
	e.xany.window = window;
	e.xclient.message_type = NET_WM_FULLSCREEN_MONITORS;
	e.xclient.format = 32;
	e.xclient.data.l[0] = 0;
	e.xclient.data.l[1] = 0;
	e.xclient.data.l[2] = uWidth;
	e.xclient.data.l[3] = uHeight;
	e.xclient.data.l[4] = 0;

	XSendEvent(display, RootWindow(display, screen), 0, SubstructureNotifyMask | SubstructureRedirectMask, &e);
	
	e.xany.type = ClientMessage;
	e.xany.window = window;
	e.xclient.message_type = NET_WM_STATE;
	e.xclient.format = 32;
	e.xclient.data.l[0] = 1;		// flag to set or unset fullscreen state
	e.xclient.data.l[1] = NET_WM_STATE_FULLSCREEN;
	e.xclient.data.l[2] = 0;
	e.xclient.data.l[3] = 0;
	e.xclient.data.l[4] = 0;
	
	XSendEvent(display, RootWindow(display, screen), 0, SubstructureNotifyMask | SubstructureRedirectMask, &e);
	
	XFlush(display);
	
	return 1;
}


/*
 *  Unset window from fullscreen popup mode to normal state
 *  If uWidth and uHeight equal zero then window
 *  dimentions sets to fullscreen
 *  Returns 1 on success or -1 on fail
 */
int32_t cGLXWindow::
unset_window_fullscreen_popup(uint32_t uWidth, uint32_t uHeight)
{
	if(!glc)
		return -1;
	
	if(uWidth == 0 && uHeight == 0)
	{
		if (1 > get_screen_resolution(&uWidth, &uHeight) )
		{
			fprintf(stderr, "Could not get screen resolution\n");
			return -1;
		}
	}
	else if(uWidth == 0 || uHeight == 0)
	{
		fprintf(stderr, "Invalid screen dimentions\n");
		return -1;
	}
	
	XEvent		e;
	Atom		NET_WM_STATE = XInternAtom(display, "_NET_WM_STATE",	0);
	Atom		NET_WM_STATE_FULLSCREEN = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);

	e.xany.type = ClientMessage;
	e.xany.window = window;
	e.xclient.message_type = NET_WM_STATE;
	e.xclient.format = 32;
	e.xclient.data.l[0] = 0;		// flag to set or unset fullscreen state
	e.xclient.data.l[1] = NET_WM_STATE_FULLSCREEN;
	e.xclient.data.l[2] = 0;
	e.xclient.data.l[3] = 0;
	e.xclient.data.l[4] = 0;
	
	XSendEvent(display, RootWindow(display, screen), 0, SubstructureNotifyMask | SubstructureRedirectMask, &e);
	XMoveResizeWindow(display, window, 0, 0, uWidth, uHeight);
	XMapRaised(display, window);
	
	XFlush(display);
	
	return 1;
}

/*
 *	Resize window to uWidth and uHeight dimentions
 *  If uWidth and uHeight equal zero then window
 *  dimentions sets to fullscreen
 *  Returns 1 on success or -1 on fail
 */
int32_t cGLXWindow::
set_window_size(uint32_t uWidth, uint32_t uHeight)
{
	if(!glc)
		return -1;
	
	if(uWidth == 0 && uHeight == 0)
	{
		if (1 > get_screen_resolution(&uWidth, &uHeight) )
		{
			fprintf(stderr, "Could not get screen resolution\n");
			return -1;
		}
	}
	else if(uWidth == 0 || uHeight == 0)
	{
		fprintf(stderr, "Invalid screen dimentions\n");
		return -1;
	}
	
	XMoveResizeWindow(display, window, 0, 0, uWidth, uHeight);
	XMapRaised(display, window);
	
	XFlush(display);
	
	return 1;
}


/*
 * Returns current window XID (window)
 */
Window cGLXWindow::
get_window_XID()
{
	return window;
}

/*
 * Returns current window Dispalay
 */
Display* cGLXWindow::
get_window_display()
{
	return display;
}

/*
 * Updating window events, running callbacks
 * Returns 1 on success and -1 if window was closed or not created
 */
int32_t cGLXWindow::
update_window(void)
{
	if(!glc)
		return -1;
	
	/* handle the events in the queue */
	if (XPending(display) > 0)
	{
		XNextEvent(display, &event);
		
		if(events_cb)
			events_cb(&event);
		
		switch (event.type)
		{
			case Expose:
				if (event.xexpose.count != 0)
					break;

				redraw_cb(&ws, 0);
				
				glXSwapBuffers(display, window);
			return 1;
			case ConfigureNotify:
				/* Set resize flag only if window size was changed */
				if ((event.xconfigure.width != ws.uWidth) || 
					(event.xconfigure.height != ws.uHeight))
				{
					ws.uWidth = event.xconfigure.width;
					ws.uHeight = event.xconfigure.height;
						
					redraw_cb(&ws, VIEWPORT_RESIZE_FLAG);
					
					glXSwapBuffers(display, window);
				}
				
			return 1;
			case ClientMessage:
//				if (event.xclient.data.l[0] == wmDelete)
//					destroy_window();
            return -1;
			case ButtonPress:
                    
            return 1;
			case ButtonRelease:

            return 1;
            case KeyPress:
				if(events_cb)
					return 1;

				if (XLookupKeysym(&event.xkey, 0) == XK_Escape)
				{
					destroy_window();
					return -1;
				}
			return 1;
            case KeyRelease:
				
			return 1;
		}
		
	}
	else
	{
		/* Simple redraw GL window */
		/* Getting mouse cursor position */
		XQueryPointer(
						display, 
						window, 
						&returnedWindow,
						&returnedWindow, 
						&ws.iMouseRootX, 
						&ws.iMouseRootY, 
						&ws.iMouseWinX, 
						&ws.iMouseWinY,
						&uMask
					);
	
		redraw_cb(&ws, 0);
		glXSwapBuffers(display, window);
	}
	
	return 1;
}

#endif /* GLXWINDOW_H */

