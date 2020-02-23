/*
 *  Copyright (c) 2013 - 2020 Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
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
 * File:   screensaver_3dclock.cpp
 * Author: Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *
 * Created on 23 января 2020 г., 13:16
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <linux/random.h>
#include <errno.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h> 

#include <wchar.h> 
#include <pthread.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "utils/time_func.h"
#include "buffers/discrete_ring_buffer.h"
#include "graphic/glxwindow.h"




uint32_t	const	ZERO_OFFSET = '0';
uint32_t	const	NUM_OF_DIGITS = 10;
uint32_t	const	FONT_HEIGHT = 128;

uint32_t	const	BITMAP_WIDTH = 256;
uint32_t	const	BITMAP_HEIGHT = 256;

uint32_t	const	FLAME_WIDTH = 1024;
uint32_t	const	FLAME_HEIGHT = 512;
uint32_t	const	FLAME_RATE_MILLIS = 20;
float		const	FLAME_DEVIDER = 2.97;
uint32_t	const	RBUFFER_LEN = 5;

float		const	CUBE_ROTATION_SPEED = 0.006;

char		const	FONT_NAME[]="./fonts/Roboto-Bold.ttf";
char		const	*FRAGMENT_PROGRAM =
"uniform vec4 color;\n" 
"void main() {\n" 
" gl_FragColor = color;\n" 
"}\n";



struct	sRenderVariables
{
	time_t				rawtime;
	tm					tminfo;
	uint32_t			uHour;
	uint32_t			uMin;
	uint32_t			uSec;
	char				cTimeBuff[4];
	float				fX;
	int32_t				iStartMousePosX;
	int32_t				iStartMousePosY;
	float				fRangeZ;
	float				fAngleX[3];
	float				fAngleY[3];
	GLuint				uFlameTex;
	GLuint				uTimeTex[3];
	uint8_t				flameBuff[FLAME_WIDTH*FLAME_HEIGHT*3];
	size_t				uDataSize;
	uint64_t			uPrevMillis;
	uint64_t			uCurrMillis;
	float				fDelta;
	GLUquadricObj		*quadrObj;
	int32_t				iSignY;
};


struct	rgb	
{
	uint8_t		r;
	uint8_t		g;
	uint8_t		b;
};

struct	sSymbol
{
	uint16_t	uWidth;
	uint16_t	uHeight;
	uint16_t	uTop;
	int16_t		iLeft;
	GLuint		texture;
};



sSymbol				_symbol[NUM_OF_DIGITS];
uint8_t				_appExit = 0;
cDiscreteRingBuffer	_rb;
sRenderVariables	_rv = {0};



/* Filling buffer with pseudo random byte values from /dev/random */
inline int32_t 
get_random(void* pBuff, const size_t buffSize)
{
	int32_t	hFile = -1;
	int32_t	iBytesRead;
	if ((hFile = open("/dev/urandom", O_RDONLY )) < 0)
	{
		return hFile;
	}
	
	iBytesRead = read(hFile, pBuff, buffSize);

	close(hFile);
	return iBytesRead;
}

inline int32_t
randval(uint32_t uMaxVal)
{
	uint32_t uRandVal;
	if(sizeof(uRandVal) != get_random(&uRandVal, sizeof(uRandVal) ))
		return -1;

	return (uint64_t)uRandVal*(uMaxVal+1)/0xffffffff;
}


void* 
creating_flame_thread(void*)
{
	uint32_t		uIndex;
	uint32_t		i,j;
	uint8_t		tmp, color;
	rgb		palit[256] = {0};
	uint8_t		palBuff [FLAME_WIDTH*FLAME_HEIGHT];
	rgb		flameBuff [FLAME_HEIGHT*FLAME_WIDTH];
	uint8_t		pseudoRandArray[FLAME_WIDTH*FLAME_HEIGHT+1];
	size_t		uRandIndex = 0;
	
	uint64_t		uPrevMillis = get_millisec();
	uint64_t		uCurrMillis;
	
	/* Filling pseudo random array */
	get_random(pseudoRandArray, sizeof(pseudoRandArray));

	/* Filling flame palit */
	for(uIndex=0; uIndex<64; uIndex++)
	{
		palit[uIndex].r=uIndex*4;
		palit[uIndex+64].r=255;
		palit[uIndex+64].g=uIndex*4;
		palit[uIndex+128].r=255;
		palit[uIndex+128].g=255;
		palit[uIndex+128].b=uIndex*4;
		palit[uIndex+192].r=255;
		palit[uIndex+192].g=255;
		palit[uIndex+192].b=255;
	}
	
	/* Flame updating */
	while(_appExit == 0)
	{
		sleep_millisec(5);
		
		uCurrMillis = get_millisec();
		if( FLAME_RATE_MILLIS >= (uCurrMillis - uPrevMillis)
			|| RBUFFER_LEN <= _rb.get_current_len())
			continue;
			
		uPrevMillis = uCurrMillis;
		
		/* Flame seeds */
		for(i=0; i<FLAME_WIDTH; i+=4)
		{
			color = pseudoRandArray[uRandIndex++];
			for(j=0; j<4; j++)
			{
				palBuff[i+j]=color;
			}
			
			if(uRandIndex >= sizeof(pseudoRandArray) )
				uRandIndex = 0;
		}
		
		/* Two passes flame generation method */
		for(i=1; i<FLAME_WIDTH-1; i++)
		{
			for(j=1; j<FLAME_HEIGHT; j++)
			{
				tmp =( 
						palBuff[(i-1)+(j-1)*FLAME_WIDTH] 
						+ palBuff[i+(j-1)*FLAME_WIDTH]
						+ palBuff[(i+1)+(j-1)*FLAME_WIDTH] 
					)/FLAME_DEVIDER;
				
				if(tmp > 1) 
					tmp--; 
				else 
					tmp=0;
				
				palBuff[i+FLAME_WIDTH*j]=tmp;
			}
		}
		
		for(i=FLAME_WIDTH-2; i > 1; i--)
		{
			for(j=1; j<FLAME_HEIGHT; j++)
			{
				tmp =( 
						palBuff[((j-1)*FLAME_WIDTH)+(i-1)] 
						+ palBuff[((j-1)*FLAME_WIDTH) + i]
						+ palBuff[((j-1)*FLAME_WIDTH)+(i+1)] 
					)/FLAME_DEVIDER;
				
				if(tmp > 1) 
					tmp--; 
				else 
					tmp=0;
				
				palBuff[i+FLAME_WIDTH*j]=tmp;
			}
		}
		
		/* Fill rgb array from palit buffer */
		for(i=0; i<FLAME_WIDTH; i++)
		{
			for(j=0; j<FLAME_HEIGHT; j++)
			{
				flameBuff[i+j*FLAME_WIDTH]=palit[palBuff[i+j*FLAME_WIDTH]];
			}
		}
		
		/* Write completed flame array to ring buffer */
		_rb.write(flameBuff, sizeof(flameBuff) );
		
	}

	pthread_exit(NULL);
}




/* Generates digits textures from tt fonts */
int32_t	
create_digits_tex_array( void )
{
	FT_Library		lib;
	FT_Face			font;
	FT_GlyphSlot	gliph;
	uint8_t			*bitmap = NULL;
	
	memset(_symbol, 0, sizeof(_symbol) );

	if(0 != FT_Init_FreeType(&lib))
	{
		fprintf(stderr, "FT_Init_FreeType: Error init freetype library\n\r");
		return 0;
	}
	
	if(0 != FT_New_Face(lib, FONT_NAME, 0, &font))
	{
		fprintf(stderr, "FT_New_Face: Font loading error\n");
		return 0;
	}
	
	if(0 != FT_Set_Char_Size(font, FONT_HEIGHT << 6, FONT_HEIGHT << 6, 96, 96))
	{
		fprintf(stderr, "FT_Set_Pixel_Sizes: Error set size of pixel\n");
		return 0;		
	}
	
	
	gliph = font->glyph;
	
	for(uint32_t i = 0; i<NUM_OF_DIGITS; i++)
	{
		if(0 != FT_Load_Char(font, i+ZERO_OFFSET, FT_LOAD_RENDER))
		{
			fprintf(stderr, "FT_Load_Char: Error load char\n");
		}
			
		_symbol[i].uWidth = gliph->bitmap.width;
		_symbol[i].uHeight = gliph->bitmap.rows;
		_symbol[i].uTop = gliph->bitmap_top;
		_symbol[i].iLeft = gliph->bitmap_left;

		/* Two bytes for each pixel */
		uint32_t	uiBitmapSize = 2*_symbol[i].uWidth*_symbol[i].uHeight;	
		bitmap = NULL;
		bitmap = (uint8_t*)malloc(uiBitmapSize);
		if(bitmap == NULL)
		{
			fprintf(stderr, "malloc: Error memory allocating fot bimap\n");
			return 0;
		}
		
		memset(bitmap, 255, uiBitmapSize);
		
		/* Filling bitmap grey&alpha bytes */
		for(uint32_t y=0; y<_symbol[i].uHeight; y++)
		{
			for(uint32_t x=0; x<_symbol[i].uWidth; x++)
			{
				bitmap[2*(x+y*gliph->bitmap.width)+1]
					= 0.97*gliph->bitmap.buffer[x+y*gliph->bitmap.width];
			}

		}
		
		/* Creating symbol texture */
		glGenTextures(1, &_symbol[i].texture);
		if(0 == _symbol[i].texture)
		{
			fprintf(stderr, "glGenTextures: Error generate texture indentifier (GL not init?)\n");
			return 0;
		}
		
			
		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_RECTANGLE, _symbol[i].texture);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D( GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 
						_symbol[i].uWidth, _symbol[i].uHeight, 
						0, GL_LUMINANCE_ALPHA,
						GL_UNSIGNED_BYTE, bitmap);
		
		
		free(bitmap);
	}
	
	FT_Done_Face(font);
	FT_Done_FreeType(lib);
	
	return 1;
}

void
destroy_digits_tex_array(void)
{
	for(uint32_t i=0; i<NUM_OF_DIGITS; i++)
	{
		glDeleteTextures(1, &_symbol[i].texture);
	}
	
	memset(_symbol, 0, sizeof(_symbol));
}


/* Draw digit textured quads */
void
draw_gliph_quads(const char *szTxt)
{
	uint32_t uTxtLen = strlen(szTxt);
	if(uTxtLen != 2)
		return;
	
	int32_t	x = ( BITMAP_WIDTH - 2*_symbol[0].uWidth-_symbol[0].iLeft	) *0.45;
	int32_t	y = (BITMAP_HEIGHT-FONT_HEIGHT)/2;
	
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glNormal3f( 0.0f,  0.0f, 1.0f);
	
	for(uint32_t i=0; i<uTxtLen; i++)
	{
		glBindTexture(GL_TEXTURE_RECTANGLE, _symbol[szTxt[i]-ZERO_OFFSET].texture );
		
		x += _symbol[szTxt[i]-ZERO_OFFSET].iLeft + 1;
		y = y - (_symbol[szTxt[i]-ZERO_OFFSET].uHeight - _symbol[szTxt[i]-ZERO_OFFSET].uTop);
		
		glBegin(GL_QUADS);
			glTexCoord2f( _symbol[szTxt[i]-ZERO_OFFSET].uWidth-1,	0);
			glVertex3f( x+_symbol[szTxt[i]-ZERO_OFFSET].uWidth, y+_symbol[szTxt[i]-ZERO_OFFSET].uHeight, 0.5f);
			
			glTexCoord2f(0,	0);
			glVertex3f(	x, y+_symbol[szTxt[i]-ZERO_OFFSET].uHeight, 0.5f);
		
			glTexCoord2f(0,	_symbol[szTxt[i]-ZERO_OFFSET].uHeight-1);
			glVertex3f(	x, y, 0.5f);
			
			glTexCoord2f(_symbol[szTxt[i]-ZERO_OFFSET].uWidth-1,	_symbol[szTxt[i]-ZERO_OFFSET].uHeight-1);
			glVertex3f(	x+_symbol[szTxt[i]-ZERO_OFFSET].uWidth, y, 0.5f);
		glEnd();
		
		x += _symbol[szTxt[i]-ZERO_OFFSET].uWidth;
		
	}
};


void
draw_time_edge_texture(const char *szTime, GLuint uTex)
{
	uint32_t	const	uBorderWidth = 12;
	uint32_t	const	uLineWidth = 2;
	uint32_t	const	uCathet = 50;
	float		const	fLineConst = 0.7;
	
	glViewport(0, 0, BITMAP_WIDTH, BITMAP_HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, BITMAP_WIDTH, 0, BITMAP_HEIGHT, 0, 20);
	gluLookAt(0, 0, 1, 0, 0, 0, 0, 1, 0);
	
	glDisable(GL_TEXTURE_RECTANGLE);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	
	/*************** Drawing base cube edge texture background ****************/
	/* Background quad */
	glColor4f(0.0, 0.1, 0.1, 0.9f);
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, 0, 1); 
		glVertex3f( BITMAP_WIDTH-1, 0, 1);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1, 1);
		glVertex3f( 0, BITMAP_HEIGHT-1, 1);
	glEnd();
	
	/* Borders */
	glColor4f(0.1, 0.1, 1.0, 0.9f);
	/* Bottom */
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, 0, 1); 
		glVertex3f( BITMAP_WIDTH-1, 0, 1);
		glVertex3f( BITMAP_WIDTH-1, uBorderWidth, 1);
		glVertex3f( 0, uBorderWidth, 1);
	glEnd();
	/* left */
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, 0, 1); 
		glVertex3f( uBorderWidth, 0, 1);
		glVertex3f( uBorderWidth, BITMAP_HEIGHT-1, 1);
		glVertex3f( 0, BITMAP_HEIGHT-1, 1);
	glEnd();		
	/* Right */
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( BITMAP_WIDTH-1-uBorderWidth, 0, 1); 
		glVertex3f( BITMAP_WIDTH-1, 0, 1);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1, 1);
		glVertex3f( BITMAP_WIDTH-1-uBorderWidth, BITMAP_HEIGHT-1, 1);
	glEnd();	
	/* Top */
	glBegin(GL_QUADS);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, BITMAP_HEIGHT-1, 1); 
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1, 1);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1-uBorderWidth, 1);
		glVertex3f( 0, BITMAP_HEIGHT-1-uBorderWidth, 1);
	glEnd();
	
	/* Triangles */
	/* Bottom-left */
	glBegin(GL_TRIANGLES);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, 0, 1);
		glVertex3f( uCathet, 0, 1);
		glVertex3f( 0, uCathet, 1);
	glEnd();
	/* Bottom-right */
	glBegin(GL_TRIANGLES);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( BITMAP_WIDTH-1, 0, 1);
		glVertex3f( BITMAP_WIDTH-1-uCathet, 0, 1);
		glVertex3f( BITMAP_WIDTH-1, uCathet, 1);
	glEnd();
	/* Top-left */
	glBegin(GL_TRIANGLES);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( 0, BITMAP_HEIGHT-1, 1);
		glVertex3f( uCathet, BITMAP_HEIGHT-1, 1);
		glVertex3f( 0, BITMAP_HEIGHT-1-uCathet, 1);
	glEnd();
	/* Top-right */
	glBegin(GL_TRIANGLES);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1, 1);
		glVertex3f( BITMAP_WIDTH-1-uCathet, BITMAP_HEIGHT-1, 1);
		glVertex3f( BITMAP_WIDTH-1, BITMAP_HEIGHT-1-uCathet, 1);
	glEnd();
	
	/* Lines */
	glLineWidth(uLineWidth);
	glColor4f(1.0, 0.90, 0.1, 0.7f);
	glBegin(GL_LINE_LOOP);
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glVertex3f( uCathet*fLineConst, uBorderWidth, 1);
		glVertex3f( BITMAP_WIDTH-1-uCathet*fLineConst, uBorderWidth, 1);
		glVertex3f( BITMAP_WIDTH-1-uBorderWidth, uCathet*fLineConst, 1);
		glVertex3f( BITMAP_WIDTH-1-uBorderWidth, BITMAP_HEIGHT-1-uCathet*fLineConst, 1);
		glVertex3f( BITMAP_WIDTH-1-uCathet*fLineConst, BITMAP_HEIGHT-1-uBorderWidth, 1);
		glVertex3f( uCathet*fLineConst, BITMAP_HEIGHT-1-uBorderWidth, 1);
		glVertex3f( uBorderWidth, BITMAP_HEIGHT-1-uCathet*fLineConst, 1);
		glVertex3f( uBorderWidth, uCathet*fLineConst, 1);
	glEnd();
	
	glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
	
	draw_gliph_quads(szTime);
	
	glEnable(GL_TEXTURE_RECTANGLE);
	glBindTexture(GL_TEXTURE_RECTANGLE, uTex);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 0, 0, BITMAP_WIDTH, BITMAP_HEIGHT, 0);
	
}


/* Redraw window callback */
void
redraw_window(cGLXWindow::sWindowState *ws, uint32_t stateFlags)
{
	int32_t		i;
	GLfloat		light0Diffuse[] = { 1.0, 1.0, 1.0 };
	GLfloat		light0Ambient[] = { 0.3, 0.3, 0.3 };
	GLfloat		light0Direction[] = { 0.0, 0.0, 1.0, 0.0 };

	/************************ GL initializing *********************************/
	if (cGLXWindow::VIEWPORT_INIT_FLAG & stateFlags)
	{
		_rv.uPrevMillis = get_millisec();
		
		glEnable(GL_POLYGON_SMOOTH);
		glEnable(GL_LINE_SMOOTH);
		glShadeModel(GL_SMOOTH);
		glEnable(GL_NORMALIZE);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_MULTISAMPLE);
		
		glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
		glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );

		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_TEXTURE_RECTANGLE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		/* Init sphere objects */
		_rv.quadrObj = gluNewQuadric();
		gluQuadricDrawStyle(_rv.quadrObj,GLU_FILL);
		gluQuadricNormals(_rv.quadrObj, GLU_SMOOTH);
		
		/* Getting start random rotation angles for cubes */
		for(i=0; i<3; i++)
		{
			_rv.fAngleY[i]=(float)(randval(60));
			_rv.fAngleX[i]=(float)(randval(360));
		}

		/* Generating textures */
		create_digits_tex_array();

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenTextures(1, &_rv.uFlameTex);
		for(i=0; i<3; i++)
			glGenTextures(1, &_rv.uTimeTex[i]);
		
		/************************* Creating lists of objects **********************/
		/* Digits cube */
		glNewList(1, GL_COMPILE);
			glBegin(GL_QUADS);
			glNormal3f( 0.0, 0.0, 1.0);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f( 0.5, 0.5, 0.5);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5, 0.5, 0.5);
			glTexCoord2f(0,0);							glVertex3f(-0.5,-0.5, 0.5);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5,-0.5, 0.5);

			glNormal3f( 0.0, 0.0,-1.0);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5,-0.5,-0.5);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5, 0.5,-0.5);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5, 0.5,-0.5);
			glTexCoord2f(0,0);							glVertex3f( 0.5,-0.5,-0.5);

			glNormal3f( 0.0, 1.0, 0.0);
			glTexCoord2f(0,0);							glVertex3f( 0.5, 0.5, 0.5);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5, 0.5,-0.5);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5, 0.5,-0.5);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5, 0.5, 0.5);

			glNormal3f( 0.0,-1.0, 0.0);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5,-0.5,-0.5);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5,-0.5,-0.5);
			glTexCoord2f(0,0);							glVertex3f( 0.5,-0.5, 0.5);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5,-0.5, 0.5);

			glNormal3f( 1.0, 0.0, 0.0);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f( 0.5, 0.5, 0.5);
			glTexCoord2f(0,0);							glVertex3f( 0.5,-0.5, 0.5);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f( 0.5,-0.5,-0.5);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f( 0.5, 0.5,-0.5);

			glNormal3f(-1.0, 0.0, 0.0);
			glTexCoord2f(0,0);							glVertex3f(-0.5,-0.5,-0.5);
			glTexCoord2f(BITMAP_WIDTH,0);				glVertex3f(-0.5,-0.5, 0.5);
			glTexCoord2f(BITMAP_WIDTH,BITMAP_HEIGHT);	glVertex3f(-0.5, 0.5, 0.5);
			glTexCoord2f(0,BITMAP_HEIGHT);				glVertex3f(-0.5, 0.5,-0.5);
			glEnd();
		glEndList();

		/* Flame cube */
		glNewList(2, GL_COMPILE);
			glBegin(GL_QUADS);
			glNormal3f( 0.0, 0.0, 1.0);
			glNormal3f( 0.0, 0.0, 1.0);
			glTexCoord2f(FLAME_WIDTH, FLAME_HEIGHT);	glVertex3f( 0.5, 0.5, 0.5);
			glTexCoord2f(0, FLAME_HEIGHT);				glVertex3f(-0.5, 0.5, 0.5);
			glTexCoord2f(0, 0);							glVertex3f(-0.5,-0.5, 0.5);
			glTexCoord2f(FLAME_WIDTH, 0);				glVertex3f( 0.5,-0.5, 0.5);

			glNormal3f( 0.0, 0.0,-1.0);
			glTexCoord2f(FLAME_WIDTH, 0);				glVertex3f(-0.5,-0.5,-0.5);
			glTexCoord2f(FLAME_WIDTH, FLAME_HEIGHT);	glVertex3f(-0.5, 0.5,-0.5);
			glTexCoord2f(0, FLAME_HEIGHT);				glVertex3f( 0.5, 0.5,-0.5);
			glTexCoord2f(0, 0);							glVertex3f( 0.5,-0.5,-0.5);

			glNormal3f( 0.0, 1.0, 0.0);
			glTexCoord2f(0, 0);							glVertex3f( 0.5, 0.5, 0.5);
			glTexCoord2f(FLAME_WIDTH, 0);				glVertex3f( 0.5, 0.5,-0.5);
			glTexCoord2f(FLAME_WIDTH, FLAME_HEIGHT);	glVertex3f(-0.5, 0.5,-0.5);
			glTexCoord2f(0, FLAME_HEIGHT);				glVertex3f(-0.5, 0.5, 0.5);

			glNormal3f( 0.0,-1.0, 0.0);
			glTexCoord2f(FLAME_WIDTH, FLAME_HEIGHT);	glVertex3f(-0.5,-0.5,-0.5);
			glTexCoord2f(0, FLAME_HEIGHT);				glVertex3f( 0.5,-0.5,-0.5);
			glTexCoord2f(0, 0);							glVertex3f( 0.5,-0.5, 0.5);
			glTexCoord2f(FLAME_WIDTH, 0);				glVertex3f(-0.5,-0.5, 0.5);

			glNormal3f( 1.0, 0.0, 0.0);
			glTexCoord2f(0, FLAME_HEIGHT);				glVertex3f( 0.5, 0.5, 0.5);
			glTexCoord2f(0, 0);							glVertex3f( 0.5,-0.5, 0.5);
			glTexCoord2f(FLAME_WIDTH, 0);				glVertex3f( 0.5,-0.5,-0.5);
			glTexCoord2f(FLAME_WIDTH, FLAME_HEIGHT);	glVertex3f( 0.5, 0.5,-0.5);

			glNormal3f(-1.0, 0.0, 0.0);
			glTexCoord2f(0, 0);							glVertex3f(-0.5,-0.5,-0.5);
			glTexCoord2f(FLAME_WIDTH, 0);				glVertex3f(-0.5,-0.5, 0.5);
			glTexCoord2f(FLAME_WIDTH, FLAME_HEIGHT);	glVertex3f(-0.5, 0.5, 0.5);
			glTexCoord2f(0, FLAME_HEIGHT);				glVertex3f(-0.5, 0.5,-0.5);
			glEnd();
		glEndList();

		/* Spheres */
		glNewList(3, GL_COMPILE);
			for(i=-1; i<2; i+=2)
			{
				glColor3f(0.4f, 0.2f, 0.2f);
				glPushMatrix();
					glNormal3f( 0.0, 0.0, 1.0);
					glTranslatef(i, 0.2, _rv.fRangeZ);
					gluSphere(_rv.quadrObj, 0.1, 16, 16);
				glPopMatrix();

				glPushMatrix();
					glNormal3f( 0.0, 0.0, 1.0);
					glTranslatef(i, -0.2, _rv.fRangeZ);
					gluSphere(_rv.quadrObj, 0.1, 16, 16);
				glPopMatrix();
			}
		glEndList();

		glFlush();
	}
	

	
	/************************* Resizing viewport ******************************/
	if (cGLXWindow::VIEWPORT_RESIZE_FLAG & stateFlags)
	{
		_rv.fX = (float)ws->uWidth/(float)ws->uHeight;
	}
	
	/************************** Closing window ********************************/
	if (cGLXWindow::VIEWPORT_DESTROY_FLAG & stateFlags)
	{
		destroy_digits_tex_array();
	}
	
	
	/* Checking mouse cursor position and stop program if it changes */
	if(ws->iMouseRootX > 0 && ws->iMouseRootY > 0
		&& _rv.iStartMousePosX == 0xffffffff && _rv.iStartMousePosY == 0xffffffff)
	{
		_rv.iStartMousePosX = ws->iMouseRootX;
		_rv.iStartMousePosY = ws->iMouseRootY;
	}
	else if((_rv.iStartMousePosX != 0xffffffff || _rv.iStartMousePosY != 0xffffffff)
			&& ((_rv.iStartMousePosX != ws->iMouseRootX) || (_rv.iStartMousePosY != ws->iMouseRootY)) )
	{
//		_appExit = 1;
	}

	/************************* Render to texture ******************************/
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	time(&_rv.rawtime);
	localtime_r(&_rv.rawtime, &_rv.tminfo);
	
	/* Create framing digits on edges textures */
	if(_rv.uSec != _rv.tminfo.tm_sec)
	{
		snprintf(_rv.cTimeBuff, sizeof(_rv.cTimeBuff), "%0.2d", _rv.tminfo.tm_sec);
		draw_time_edge_texture(_rv.cTimeBuff, _rv.uTimeTex[2]);
		_rv.uSec = _rv.tminfo.tm_sec; 
		
		if(_rv.uMin != _rv.tminfo.tm_min)
		{
			snprintf(_rv.cTimeBuff, sizeof(_rv.cTimeBuff), "%0.2d", _rv.tminfo.tm_min);
			draw_time_edge_texture(_rv.cTimeBuff, _rv.uTimeTex[1]);
			_rv.uMin = _rv.tminfo.tm_min; 
			
			if(_rv.uHour != _rv.tminfo.tm_hour)
			{
				snprintf(_rv.cTimeBuff, sizeof(_rv.cTimeBuff), "%0.2d", _rv.tminfo.tm_hour);
				draw_time_edge_texture(_rv.cTimeBuff, _rv.uTimeTex[0]);
				_rv.uHour = _rv.tminfo.tm_hour; 
			}
		}
		
	}
	
	glFlush();

	/*********************** Render to screen *********************************/
	/* Rotations calculation */
	_rv.uCurrMillis = get_millisec();
	_rv.fDelta = CUBE_ROTATION_SPEED * float(_rv.uCurrMillis - _rv.uPrevMillis);
//	printf("%ld\r\n", uCurrMillis - uPrevMillis);
	_rv.uPrevMillis = _rv.uCurrMillis;
	
	for(i=0; i<3; i++)
	{
		_rv.fAngleX[i] = (float)_rv.fAngleX[i] + _rv.fDelta;
		
		if(_rv.fAngleX[i] > 360)
			_rv.fAngleX[i]=0;

		_rv.fAngleY[i] = _rv.fAngleY[i] + _rv.fDelta*_rv.iSignY;
		
		if((_rv.fAngleY[i] > 60) || (_rv.fAngleY[i] < -60)) 
			_rv.iSignY *= -1;
	}
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(50,(double)ws->uWidth / ws->uHeight,0.5,500);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, 6, 0, 0, 0, 0, 1, 0);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, ws->uWidth, ws->uHeight);

	glLoadIdentity();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glEnable(GL_TEXTURE_RECTANGLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	/* Drawing flame quad */
	glBindTexture(GL_TEXTURE_RECTANGLE, _rv.uFlameTex);
	
	/* Getting next flame rgb array from ring buffer */
	if(1 == _rb.read(_rv.flameBuff, sizeof(_rv.flameBuff), &_rv.uDataSize))
	{
		glTexImage2D(GL_TEXTURE_RECTANGLE, 0, 3, FLAME_WIDTH, FLAME_HEIGHT, 
							 0, GL_RGB, GL_UNSIGNED_BYTE, _rv.flameBuff);
	}
	
	glDisable(GL_LIGHTING);
	
	glColor4f(1.0,1.0,1.0,0.9f);
    glPushMatrix();
		glTranslatef(0.0f, 0.0f, -10.0f);
			glBegin(GL_QUADS);
			glNormal3f( 0.0, 0.0, 1.0);
			glTexCoord2f(FLAME_WIDTH,			0.0);	glVertex3f(-5.0*_rv.fX,-5.2, 0.0);
			glTexCoord2f(FLAME_WIDTH,	FLAME_HEIGHT);	glVertex3f(-5.0*_rv.fX, 4.0, 0.0);
			glTexCoord2f(0.0,			FLAME_HEIGHT);	glVertex3f( 5.0*_rv.fX, 4.0, 0.0);
			glTexCoord2f(0.0,			0.0);			glVertex3f( 5.0*_rv.fX,-5.2, 0.0);
			glEnd();
	glPopMatrix();
	
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light0Ambient);
	glLightfv(GL_LIGHT0, GL_POSITION, light0Direction);

	glEnable(GL_COLOR_MATERIAL);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
	
	/* Drawing time cubes */
	glColor4f(1.0f,1.0f,1.0f,1.0f);
	
	for(i=0; i<3; i++)
	{
		glPushMatrix();
			glTranslatef(i*2-2, 0.0, _rv.fRangeZ);
			glRotatef(_rv.fAngleY[i], 1.0, 0.0, 0.0);
			glRotatef(_rv.fAngleX[i], 0.0, 1.0, 0.0);
			glBindTexture(GL_TEXTURE_RECTANGLE, _rv.uTimeTex[i]);
			glCallList(1);
		glPopMatrix();
	}

	/* Drawing flame reflections on cube edges */
	/* Getting path of full flame texture */
	glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	glBindTexture(GL_TEXTURE_RECTANGLE, _rv.uFlameTex);
	glBlendFunc(GL_ONE, GL_ONE);
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glScalef(0.3, 0.7, 1.0);
	glMatrixMode(GL_MODELVIEW);
	
	glColor4f(0.2f, 0.1f, 0.1f, 0.5f);
	for(i=0; i<3; i++)
	{
		glPushMatrix();
			glTranslatef(i*2-2, 0.0, _rv.fRangeZ);
			glRotatef(_rv.fAngleY[i], 1.0, 0.0, 0.0);
			glRotatef(_rv.fAngleX[i], 0.0, 1.0, 0.0);
			glCallList(2);
		glPopMatrix();
	}
	
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	
	glMatrixMode(GL_MODELVIEW);

	/* Drawing spheres between cubes */
    glDisable(GL_TEXTURE_RECTANGLE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

	glCallList(3);

		
	glFlush();

}

void events_update(XEvent *event)
{
	switch (event->type)
	{
		case ButtonPress:
		case KeyPress:
			_appExit = 1;
		return;
	}
}


void init_render_variables(sRenderVariables	*rv)
{

	rv->uHour = 0xffffffff;
	rv->uMin = 0xffffffff;
	rv->uSec = 0xffffffff;
	rv->iStartMousePosX = 0xffffffff;
	rv->iStartMousePosY = 0xffffffff;
	rv->fRangeZ = -5.5F;
	rv->iSignY = 1;
}


/*
 * 
 */
int main(int argc, char** argv)
{
	cGLXWindow					window;
	cGLXWindow::sWinGLXParam	param;
	pthread_attr_t				ptAttr;
	pthread_t					ptFlame = 0;
	const char					*szExtList = NULL;
	
	param.callback_redraw = redraw_window;
	param.callback_event = events_update;
	param.uWidth = 800;
	param.uHeight = 600;
	param.iMajorGLVer = 2;
	param.iMinorGLVer = 1;
	
	_rb.create(FLAME_WIDTH*FLAME_HEIGHT*3, RBUFFER_LEN);
	
	init_render_variables(&_rv);

	window.create_window(&param, "3dclock");
	
	pthread_attr_init(&ptAttr);
	pthread_create(&ptFlame, &ptAttr, creating_flame_thread, NULL);

	while (0 < window.update_window() && _appExit == 0)
	{
		sleep_millisec(5);
	}
	
	window.destroy_window();
	_appExit = 1;

	if(ptFlame)
		pthread_join(ptFlame, NULL);
	
	pthread_attr_destroy(&ptAttr);
	
	window.show_cursor();

	return 0;
}

