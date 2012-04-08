/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#	include "SDL_syswm.h"
#else
#	include <SDL.h>
#	include <SDL_syswm.h>
#endif

#ifdef SMP
#	ifdef USE_LOCAL_HEADERS
#		include "SDL_thread.h"
#	else
#		include <SDL_thread.h>
#	endif
#ifdef SDL_VIDEO_DRIVER_X11
#	include <X11/Xlib.h>
#endif
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "tr_local.h"
#include "../client/client.h"
#include "../sys/sys_local.h"
#include "../sdl/sdl_icon.h"

/* Just hack it for now. */
#ifdef MACOS_X
#include <OpenGL/OpenGL.h>
typedef CGLContextObj QGLContext;

static void GLimp_GetCurrentContext( QGLContext *ctx )
{
	*ctx = CGLGetCurrentContext();
}

static void GLimp_SetCurrentContext( QGLContext *ctx )
{
	CGLSetCurrentContext( ctx ? *ctx : NULL );
}
static void GLimp_CreateSharedContext( QGLContext *old, qboolean debug,
				       qboolean nodraw, QGLContext *new ) {
	// AFAIK debug contexts are not supported
	CGLCreateContext ( CGLGetPixelFormat( *old ), *old, new );

	if( nodraw ) {
		glDrawBuffer( GL_NONE );
	}
}
static void GLimp_DestroyContext( QGLContext *parent, QGLContext *ctx ) {
	if( *parent != *ctx ) {
		CGLDestroyContext( *ctx );
		*ctx = *parent;
	}
}
#elif SDL_VIDEO_DRIVER_X11
#include <GL/glx.h>
typedef struct
{
	GLXContext      ctx;
	Display         *dpy;
	GLXDrawable     drawable;
} QGLContext;

static void GLimp_GetCurrentContext( QGLContext *ctx )
{
	ctx->ctx = glXGetCurrentContext();
	ctx->dpy = glXGetCurrentDisplay();
	ctx->drawable = glXGetCurrentDrawable();
}

static void GLimp_SetCurrentContext( QGLContext *ctx )
{
	if( ctx )
		glXMakeCurrent( ctx->dpy, ctx->drawable, ctx->ctx );
	else
		glXMakeCurrent( glXGetCurrentDisplay(), None, NULL );
}
static void GLimp_CreateSharedContext( QGLContext *old, qboolean debug,
				       qboolean nodraw, QGLContext *new ) {
	GLubyte *procName = (GLubyte *)"glXCreateContextAttribsARB";
	GLXContext (APIENTRYP glXCreateContextAttribsARB)( Display *dpy,
							   GLXFBConfig config,
							   GLXContext share_context,
							   Bool direct,
							   const int *attrib_list);
	int config_attrib_list[] = {
		GLX_FBCONFIG_ID, 0,
		None
	};
	int pbuffer_attrib_list[] = {
		None
	};
	int attrib_list[] = {
		GLX_RENDER_TYPE, GLX_RGBA_TYPE,
		GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
		None
	};
	GLXFBConfig *config;
	int         count;

	if( glXQueryContext(old->dpy, old->ctx,
			    GLX_FBCONFIG_ID,
			    &config_attrib_list[1]) != Success )
		return;

	config = glXChooseFBConfig( old->dpy, 0,
				    config_attrib_list,
				    &count );
	if( count != 1 )
		return;

	if( debug ) {
		glXCreateContextAttribsARB = (void *)glXGetProcAddress(procName);
		if( !glXCreateContextAttribsARB )
			debug = qfalse;
	} else {
		glXCreateContextAttribsARB = NULL;
	}

	new->dpy = old->dpy;
	new->drawable = None;
	if( nodraw )
		new->drawable = glXCreatePbuffer( old->dpy, config[0],
						  pbuffer_attrib_list );
	if( new->drawable == None )
		new->drawable = old->drawable;
	if( debug ) {
		new->ctx = glXCreateContextAttribsARB(old->dpy,
						      config[0],
						      old->ctx,
						      GL_TRUE,
						      attrib_list);
	} else {
		new->ctx = glXCreateNewContext(old->dpy,
					       config[0],
					       GLX_RGBA_TYPE,
					       old->ctx,
					       GL_TRUE);
	}

	if( nodraw ) {
		GLimp_SetCurrentContext( new );
		glDrawBuffer( GL_NONE );
	}
}
static void GLimp_DestroyContext( QGLContext *parent, QGLContext *ctx ) {
	if( parent->ctx != ctx->ctx ) {
		glXDestroyContext( ctx->dpy, ctx->ctx );
		ctx->ctx = parent->ctx;
	}
	if( parent->drawable != ctx->drawable ) {
		glXDestroyPbuffer( ctx->dpy, ctx->drawable );
		ctx->drawable = parent->drawable;
	}
}
#elif WIN32
typedef struct
{
	HDC             hDC;		// handle to device context
	HGLRC           hGLRC;		// handle to GL rendering context
} QGLContext;

static void GLimp_GetCurrentContext( QGLContext *ctx ) {
	SDL_SysWMinfo info;

	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info))
	{
		ri.Printf(PRINT_WARNING, "Failed to obtain HWND from SDL (InputRegistry)");
		return;
	}

	ctx->hDC = wglGetCurrentDC( );
	ctx->hGLRC = wglGetCurrentContext( );
}

static void GLimp_SetCurrentContext( QGLContext *ctx ) {
	if( ctx ) {
		wglMakeCurrent( ctx->hDC, ctx->hGLRC );
	} else {
		wglMakeCurrent( NULL, NULL );
	}
}
static void GLimp_CreateSharedContext( QGLContext *old, qboolean debug,
				       qboolean nodraw, QGLContext *new ) {
	LPCSTR procName = "wglCreateContextAttribsARB";
	HGLRC (APIENTRYP wglCreateContextAttribsARB) (HDC hDC,
						      HGLRC hshareContext,
						      const int *attribList);
	int attrib_list[] = {
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
		0
	};

	if( debug ) {
		wglCreateContextAttribsARB = (void *)wglGetProcAddress(procName);
		if( !wglCreateContextAttribsARB )
			debug = qfalse;
	}

	new->hDC = old->hDC;
	if( debug ) {
		new->hGLRC = wglCreateContextAttribsARB(new->hDC,
							old->hGLRC,
							attrib_list);
	} else {
		new->hGLRC = wglCreateContext(new->hDC);
		GLimp_SetCurrentContext( NULL );
		wglShareLists( old->hGLRC, new->hGLRC );
	}

	if( nodraw ) {
		GLimp_SetCurrentContext( new );
		glDrawBuffer( GL_NONE );
	}
}
static void GLimp_DestroyContext( QGLContext *parent, QGLContext *ctx ) {
	if( parent->hGLRC != ctx->hGLRC ) {
		wglDeleteContext( ctx->hGLRC );
		ctx->hGLRC = parent->hGLRC;
	}
}
#else
typedef int QGLContext;  // dummy
static void GLimp_GetCurrentContext( QGLContext *ctx ) {}
static void GLimp_SetCurrentContext( QGLContext *ctx ) {}
static void GLimp_CreateSharedContext( QGLContext *old, qboolean debug,
				       qboolean nodraw, QGLContext *new ) {}
static void GLimp_DestroyContext( QGLContext *parent, QGLContext *ctx ) {}
#endif

static QGLContext initial_context, frontend_context, backend_context;

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

static SDL_Surface *screen = NULL;
static const SDL_VideoInfo *videoInfo = NULL;

cvar_t *r_allowSoftwareGL; // Don't abort out if a hardware visual can't be obtained
cvar_t *r_allowResize; // make window resizable
cvar_t *r_centerWindow;
cvar_t *r_sdlDriver;

void (APIENTRYP qglDrawRangeElementsEXT) (GLenum mode, GLsizei count, GLuint start, GLuint end, GLenum type, const GLvoid *indices);

void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);
void (APIENTRYP qglMultiTexCoord4fvARB) (GLenum target, GLfloat *v);

void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);

// GL_ARB_vertex_buffer_object
void (APIENTRYP qglBindBufferARB) (GLenum target, GLuint buffer);
void (APIENTRYP qglDeleteBuffersARB) (GLsizei n, const GLuint *buffers);
void (APIENTRYP qglGenBuffersARB) (GLsizei n, GLuint *buffers);
GLboolean (APIENTRYP qglIsBufferARB) (GLuint buffer);
void (APIENTRYP qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
void (APIENTRYP qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);
void (APIENTRYP qglGetBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data);
GLvoid *(APIENTRYP qglMapBufferARB) (GLenum target, GLenum access);
GLboolean (APIENTRYP qglUnmapBufferARB) (GLenum target);
void (APIENTRYP qglGetBufferParameterivARB) (GLenum target, GLenum pname, GLint *params);
void (APIENTRYP qglGetBufferPointervARB) (GLenum target, GLenum pname, GLvoid **params);

// GL_ARB_map_buffer_range
GLvoid *(APIENTRYP qglMapBufferRange) (GLenum target, GLintptr offset,
				       GLsizeiptr length, GLbitfield access);
GLvoid (APIENTRYP qglFlushMappedBufferRange) (GLenum target, GLintptr offset,
					      GLsizeiptr length);

// GL_ARB_shader_objects
GLvoid (APIENTRYP qglDeleteShader) (GLuint shader);
GLvoid (APIENTRYP qglDeleteProgram) (GLuint program);
GLvoid (APIENTRYP qglDetachShader) (GLuint program, GLuint shader);
GLuint (APIENTRYP qglCreateShader) (GLenum type);
GLvoid (APIENTRYP qglShaderSource) (GLuint shader, GLsizei count, const char **string,
				    const GLint *length);
GLvoid (APIENTRYP qglCompileShader) (GLuint shader);
GLuint (APIENTRYP qglCreateProgram) (void);
GLvoid (APIENTRYP qglAttachShader) (GLuint program, GLuint shader);
GLvoid (APIENTRYP qglLinkProgram) (GLuint program);
GLvoid (APIENTRYP qglUseProgram) (GLuint program);
GLvoid (APIENTRYP qglValidateProgram) (GLuint program);
GLvoid (APIENTRYP qglUniform1f) (GLint location, GLfloat v0);
GLvoid (APIENTRYP qglUniform2f) (GLint location, GLfloat v0, GLfloat v1);
GLvoid (APIENTRYP qglUniform3f) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
GLvoid (APIENTRYP qglUniform4f) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
GLvoid (APIENTRYP qglUniform1i) (GLint location, GLint v0);
GLvoid (APIENTRYP qglUniform2i) (GLint location, GLint v0, GLint v1);
GLvoid (APIENTRYP qglUniform3i) (GLint location, GLint v0, GLint v1, GLint v2);
GLvoid (APIENTRYP qglUniform4i) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
GLvoid (APIENTRYP qglUniform1fv) (GLint location, GLsizei count, const GLfloat *value);
GLvoid (APIENTRYP qglUniform2fv) (GLint location, GLsizei count, const GLfloat *value);
GLvoid (APIENTRYP qglUniform3fv) (GLint location, GLsizei count, const GLfloat *value);
GLvoid (APIENTRYP qglUniform4fv) (GLint location, GLsizei count, const GLfloat *value);
GLvoid (APIENTRYP qglUniform1iv) (GLint location, GLsizei count, const GLint *value);
GLvoid (APIENTRYP qglUniform2iv) (GLint location, GLsizei count, const GLint *value);
GLvoid (APIENTRYP qglUniform3iv) (GLint location, GLsizei count, const GLint *value);
GLvoid (APIENTRYP qglUniform4iv) (GLint location, GLsizei count, const GLint *value);
GLvoid (APIENTRYP qglUniformMatrix2fv) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GLvoid (APIENTRYP qglUniformMatrix3fv) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GLvoid (APIENTRYP qglUniformMatrix4fv) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GLvoid (APIENTRYP qglGetShaderiv) (GLuint shader, GLenum pname, GLint *params);
GLvoid (APIENTRYP qglGetProgramiv) (GLuint program, GLenum pname, GLint *params);
GLvoid (APIENTRYP qglGetShaderInfoLog) (GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
GLvoid (APIENTRYP qglGetProgramInfoLog) (GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
GLvoid (APIENTRYP qglGetAttachedShaders) (GLuint program, GLsizei maxCount, GLsizei *count,
					  GLuint *shaders);
GLint (APIENTRYP qglGetUniformLocation) (GLuint program, const char *name);
GLvoid (APIENTRYP qglGetActiveUniform) (GLuint program, GLuint index, GLsizei maxLength,
					GLsizei *length, GLint *size, GLenum *type, char *name);
GLvoid (APIENTRYP qglGetUniformfv) (GLuint program, GLint location, GLfloat *params);
GLvoid (APIENTRYP qglGetUniformiv) (GLuint program, GLint location, GLint *params);
GLvoid (APIENTRYP qglGetShaderSource) (GLuint shader, GLsizei maxLength, GLsizei *length,
				       char *source);

// GL_ARB_vertex_shader
GLvoid (APIENTRYP qglVertexAttrib1fARB) (GLuint index, GLfloat v0);
GLvoid (APIENTRYP qglVertexAttrib1sARB) (GLuint index, GLshort v0);
GLvoid (APIENTRYP qglVertexAttrib1dARB) (GLuint index, GLdouble v0);
GLvoid (APIENTRYP qglVertexAttrib2fARB) (GLuint index, GLfloat v0, GLfloat v1);
GLvoid (APIENTRYP qglVertexAttrib2sARB) (GLuint index, GLshort v0, GLshort v1);
GLvoid (APIENTRYP qglVertexAttrib2dARB) (GLuint index, GLdouble v0, GLdouble v1);
GLvoid (APIENTRYP qglVertexAttrib3fARB) (GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
GLvoid (APIENTRYP qglVertexAttrib3sARB) (GLuint index, GLshort v0, GLshort v1, GLshort v2);
GLvoid (APIENTRYP qglVertexAttrib3dARB) (GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
GLvoid (APIENTRYP qglVertexAttrib4fARB) (GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
GLvoid (APIENTRYP qglVertexAttrib4sARB) (GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
GLvoid (APIENTRYP qglVertexAttrib4dARB) (GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
GLvoid (APIENTRYP qglVertexAttrib4NubARB) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
GLvoid (APIENTRYP qglVertexAttrib1fvARB) (GLuint index, GLfloat *v);
GLvoid (APIENTRYP qglVertexAttrib1svARB) (GLuint index, GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib1dvARB) (GLuint index, GLdouble *v);
GLvoid (APIENTRYP qglVertexAttrib2fvARB) (GLuint index, GLfloat *v);
GLvoid (APIENTRYP qglVertexAttrib2svARB) (GLuint index, GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib2dvARB) (GLuint index, GLdouble *v);
GLvoid (APIENTRYP qglVertexAttrib3fvARB) (GLuint index, GLfloat *v);
GLvoid (APIENTRYP qglVertexAttrib3svARB) (GLuint index, GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib3dvARB) (GLuint index, GLdouble *v);
GLvoid (APIENTRYP qglVertexAttrib4fvARB) (GLuint index, GLfloat *v);
GLvoid (APIENTRYP qglVertexAttrib4svARB) (GLuint index, GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib4dvARB) (GLuint index, GLdouble *v);
GLvoid (APIENTRYP qglVertexAttrib4ivARB) (GLuint index, GLint *v);
GLvoid (APIENTRYP qglVertexAttrib4bvARB) (GLuint index, GLbyte *v);
GLvoid (APIENTRYP qglVertexAttrib4ubvARB) (GLuint index, GLubyte *v);
GLvoid (APIENTRYP qglVertexAttrib4usvARB) (GLuint index, GLushort *v);
GLvoid (APIENTRYP qglVertexAttrib4uivARB) (GLuint index, GLuint *v);
GLvoid (APIENTRYP qglVertexAttrib4NbvARB) (GLuint index, const GLbyte *v);
GLvoid (APIENTRYP qglVertexAttrib4NsvARB) (GLuint index, const GLshort *v);
GLvoid (APIENTRYP qglVertexAttrib4NivARB) (GLuint index, const GLint *v);
GLvoid (APIENTRYP qglVertexAttrib4NubvARB) (GLuint index, const GLubyte *v);
GLvoid (APIENTRYP qglVertexAttrib4NusvARB) (GLuint index, const GLushort *v);
GLvoid (APIENTRYP qglVertexAttrib4NuivARB) (GLuint index, const GLuint *v);
GLvoid (APIENTRYP qglVertexAttribPointerARB) (GLuint index, GLint size, GLenum type, GLboolean normalized,
					      GLsizei stride, const GLvoid *pointer);
GLvoid (APIENTRYP qglEnableVertexAttribArrayARB) (GLuint index);
GLvoid (APIENTRYP qglDisableVertexAttribArrayARB) (GLuint index);
GLvoid (APIENTRYP qglBindAttribLocationARB) (GLhandleARB programObj, GLuint index, const GLcharARB *name);
GLvoid (APIENTRYP qglGetActiveAttribARB) (GLhandleARB programObj, GLuint index, GLsizei maxLength,
					  GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
GLint (APIENTRYP qglGetAttribLocationARB) (GLhandleARB programObj, const GLcharARB *name);
GLvoid (APIENTRYP qglGetVertexAttribdvARB) (GLuint index, GLenum pname, GLdouble *params);
GLvoid (APIENTRYP qglGetVertexAttribfvARB) (GLuint index, GLenum pname, GLfloat *params);
GLvoid (APIENTRYP qglGetVertexAttribivARB) (GLuint index, GLenum pname, GLint *params);
GLvoid (APIENTRYP qglGetVertexAttribPointervARB) (GLuint index, GLenum pname, GLvoid **pointer);

// GL_EXT_geometry_shader4
GLvoid (APIENTRYP qglProgramParameteriEXT) (GLuint program, GLenum pname, GLint value);
GLvoid (APIENTRYP qglFramebufferTextureEXT) (GLenum target, GLenum attachment,
					     GLuint texture, GLint level);
GLvoid (APIENTRYP qglFramebufferTextureLayerEXT) (GLenum target, GLenum attachment,
						  GLuint texture, GLint level, int layer);
GLvoid (APIENTRYP qglFramebufferTextureFaceEXT) (GLenum target, GLenum attachment,
						 GLuint texture, GLint level, GLenum face);

// GL_EXT_texture_buffer_object
GLvoid (APIENTRYP qglTexBufferEXT) (GLenum target, GLenum internalFormat,
				    GLuint buffer);

// GL_ARB_uniform_buffer_object
GLvoid (APIENTRYP qglGetUniformIndices) (GLuint program,
					 GLsizei uniformCount, 
					 const GLchar** uniformNames, 
					 GLuint* uniformIndices);
GLvoid (APIENTRYP qglGetActiveUniformsiv) (GLuint program,
					   GLsizei uniformCount, 
					   const GLuint* uniformIndices, 
					   GLenum pname, 
					   GLint* params);
GLvoid (APIENTRYP qglGetActiveUniformName) (GLuint program,
					    GLuint uniformIndex, 
					    GLsizei bufSize, 
					    GLsizei* length, 
					    GLchar* uniformName);
GLuint (APIENTRYP qglGetUniformBlockIndex) (GLuint program,
					    const GLchar* uniformBlockName);
GLvoid (APIENTRYP qglGetActiveUniformBlockiv) (GLuint program,
					       GLuint uniformBlockIndex, 
					       GLenum pname, 
					       GLint* params);
GLvoid (APIENTRYP qglGetActiveUniformBlockName) (GLuint program,
						 GLuint uniformBlockIndex, 
						 GLsizei bufSize, 
						 GLsizei* length, 
						 GLchar* uniformBlockName);
GLvoid (APIENTRYP qglBindBufferRange) (GLenum target, 
				       GLuint index, 
				       GLuint buffer, 
				       GLintptr offset,
				       GLsizeiptr size);
GLvoid (APIENTRYP qglBindBufferBase) (GLenum target, 
				      GLuint index, 
				      GLuint buffer);
GLvoid (APIENTRYP qglGetIntegeri_v) (GLenum target,
				     GLuint index,
				     GLint* data);
GLvoid (APIENTRYP qglUniformBlockBinding) (GLuint program,
					   GLuint uniformBlockIndex, 
					   GLuint uniformBlockBinding);

// GL_EXT_texture3D
GLvoid (APIENTRYP qglTexImage3DEXT) (GLenum target, GLint level,
				     GLenum internalformat, GLsizei width,
				     GLsizei height, GLsizei depth,
				     GLint border, GLenum format, GLenum type,
				     const GLvoid *pixels);

// GL_ARB_framebuffer_object
GLboolean (APIENTRYP qglIsRenderbuffer) (GLuint renderbuffer);
GLvoid (APIENTRYP qglBindRenderbuffer) (GLenum target, GLuint renderbuffer);
GLvoid (APIENTRYP qglDeleteRenderbuffers) (GLsizei n, const GLuint *renderbuffers);
GLvoid (APIENTRYP qglGenRenderbuffers) (GLsizei n, GLuint *renderbuffers);
GLvoid (APIENTRYP qglRenderbufferStorage) (GLenum target, GLenum internalformat,
					   GLsizei width, GLsizei height);
GLvoid (APIENTRYP qglRenderbufferStorageMultisample) (GLenum target, GLsizei samples,
						      GLenum internalformat,
						      GLsizei width, GLsizei height);
GLvoid (APIENTRYP qglGetRenderbufferParameteriv) (GLenum target, GLenum pname, GLint *params);
GLboolean (APIENTRYP qglIsFramebuffer) (GLuint framebuffer);
GLvoid (APIENTRYP qglBindFramebuffer) (GLenum target, GLuint framebuffer);
GLvoid (APIENTRYP qglDeleteFramebuffers) (GLsizei n, const GLuint *framebuffers);
GLvoid (APIENTRYP qglGenFramebuffers) (GLsizei n, GLuint *framebuffers);
GLenum (APIENTRYP qglCheckFramebufferStatus) (GLenum target);
GLvoid (APIENTRYP qglFramebufferTexture1D) (GLenum target, GLenum attachment,
					    GLenum textarget, GLuint texture, GLint level);
GLvoid (APIENTRYP qglFramebufferTexture2D) (GLenum target, GLenum attachment,
					    GLenum textarget, GLuint texture, GLint level);
GLvoid (APIENTRYP qglFramebufferTexture3D) (GLenum target, GLenum attachment,
					    GLenum textarget, GLuint texture,
					    GLint level, GLint layer);
GLvoid (APIENTRYP qglFramebufferTextureLayer) (GLenum target, GLenum attachment,
					       GLuint texture, GLint level, GLint layer);
GLvoid (APIENTRYP qglFramebufferRenderbuffer) (GLenum target, GLenum attachment,
					       GLenum renderbuffertarget, GLuint renderbuffer);
GLvoid (APIENTRYP qglGetFramebufferAttachmentParameteriv) (GLenum target, GLenum attachment,
							   GLenum pname, GLint *params);
GLvoid (APIENTRYP qglBlitFramebuffer) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
				       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
				       GLbitfield mask, GLenum filter);
GLvoid (APIENTRYP qglGenerateMipmap) (GLenum target);

// GL_EXT_occlusion_query
GLvoid (APIENTRYP qglGenQueriesARB) (GLsizei n, GLuint *ids);
GLvoid (APIENTRYP qglDeleteQueriesARB) (GLsizei n, const GLuint *ids);
GLboolean (APIENTRYP qglIsQueryARB) (GLuint id);
GLvoid (APIENTRYP qglBeginQueryARB) (GLenum target, GLuint id);
GLvoid (APIENTRYP qglEndQueryARB) (GLenum target);
GLvoid (APIENTRYP qglGetQueryivARB) (GLenum target, GLenum pname, GLint *params);
GLvoid (APIENTRYP qglGetQueryObjectivARB) (GLuint id, GLenum pname, GLint *params);
GLvoid (APIENTRYP qglGetQueryObjectuivARB) (GLuint id, GLenum pname, GLuint *params);

// GL_EXT_timer_query
GLvoid (APIENTRYP qglGetQueryObjecti64vEXT) (GLuint id, GLenum pname, GLint64EXT *params);
GLvoid (APIENTRYP qglGetQueryObjectui64vEXT) (GLuint id, GLenum pname, GLuint64EXT *params);

// GL_ARB_instanced_arrays
GLvoid (APIENTRYP qglVertexAttribDivisorARB) (GLuint index, GLuint divisor);
GLvoid (APIENTRYP qglDrawArraysInstancedARB) (GLenum mode, GLint first, GLsizei count,
					      GLsizei primcount);
GLvoid (APIENTRYP qglDrawElementsInstancedARB) (GLenum mode, GLsizei count, GLenum type,
						const GLvoid *indices, GLsizei primcount);

// GL_ARB_separate_stencil
GLvoid (APIENTRYP qglStencilOpSeparate) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
GLvoid (APIENTRYP qglStencilFuncSeparate) (GLenum face, GLenum func, GLint ref, GLuint mask);
GLvoid (APIENTRYP qglStencilMaskSeparate) (GLenum face, GLuint mask);

// GL_ARB_debug_output, not in core
GLvoid (APIENTRYP qglDebugMessageControlARB) (GLenum source,
					      GLenum type,
					      GLenum severity,
					      GLsizei count,
					      const GLuint* ids,
					      GLboolean enabled);
GLvoid (APIENTRYP qglDebugMessageInsertARB) (GLenum source,
					     GLenum type,
					     GLuint id,
					     GLenum severity,
					     GLsizei length, 
					     const GLchar* buf);
GLvoid (APIENTRYP qglDebugMessageCallbackARB) (GLDEBUGPROCARB callback,
					       GLvoid *userParam);
GLuint (APIENTRYP qglGetDebugMessageLogARB) (GLuint count,
					     GLsizei bufsize,
					     GLenum *sources,
					     GLenum *types,
					     GLuint *ids,
					     GLenum *severities,
					     GLsizei *lengths, 
					     GLchar *messageLog);
// GL_AMD_debug_output, predecessor to GL_ARB_debug_output, but has only
// a category parameter instead of source and type
GLvoid (APIENTRYP qglDebugMessageEnableAMD) (GLenum category,
					     GLenum severity,
					     GLsizei count,
					     const GLuint* ids,
					     GLboolean enabled);
GLvoid (APIENTRYP qglDebugMessageInsertAMD) (GLenum category,
					     GLuint id,
					     GLenum severity,
					     GLsizei length, 
					     const GLchar* buf);
GLvoid (APIENTRYP qglDebugMessageCallbackAMD) (GLDEBUGPROCAMD callback,
					       GLvoid *userParam);
GLuint (APIENTRYP qglGetDebugMessageLogAMD) (GLuint count,
					     GLsizei bufsize,
					     GLenum *categories,
					     GLuint *ids,
					     GLenum *severities,
					     GLsizei *lengths, 
					     GLchar *messageLog);

// GL_ARB_blend_func_extended
GLvoid (APIENTRYP qglBindFragDataLocationIndexed) (GLuint program,
						   GLuint colorNumber,
						   GLuint index,
						   const GLchar *name);
GLint (APIENTRYP qglGetFragDataIndex) (GLuint program,
				       const GLchar *name);

static unsigned renderThreadID = 0;
/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
	if( glGlobals.timerQuery ) {
		qglDeleteQueriesARB( 1, &glGlobals.timerQuery );
	}

	ri.IN_Shutdown();

	GLimp_SetCurrentContext( &initial_context );

#ifdef SMP
	if( r_smp->integer ) {
		GLimp_DestroyContext( &backend_context, &frontend_context );
	}
#endif
	if( r_ext_debug_output->integer ) {
		GLimp_DestroyContext( &initial_context, &backend_context );
	}

	SDL_QuitSubSystem( SDL_INIT_VIDEO );
	screen = NULL;

	Com_Memset( &glConfig, 0, sizeof( glConfig ) );
	Com_Memset( &glState, 0, sizeof( glState ) );
}

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize(void)
{
	SDL_WM_IconifyWindow();
}


/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( char *comment )
{
}

/*
===============
GLimp_CompareModes
===============
*/
static int GLimp_CompareModes( const void *a, const void *b )
{
	const float ASPECT_EPSILON = 0.001f;
	SDL_Rect *modeA = *(SDL_Rect **)a;
	SDL_Rect *modeB = *(SDL_Rect **)b;
	float aspectA = (float)modeA->w / (float)modeA->h;
	float aspectB = (float)modeB->w / (float)modeB->h;
	int areaA = modeA->w * modeA->h;
	int areaB = modeB->w * modeB->h;
	float aspectDiffA = fabs( aspectA - glGlobals.displayAspect );
	float aspectDiffB = fabs( aspectB - glGlobals.displayAspect );
	float aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if( aspectDiffsDiff > ASPECT_EPSILON )
		return 1;
	else if( aspectDiffsDiff < -ASPECT_EPSILON )
		return -1;
	else
		return areaA - areaB;
}


/*
===============
GLimp_DetectAvailableModes
===============
*/
static void GLimp_DetectAvailableModes(void)
{
	char buf[ MAX_STRING_CHARS ] = { 0 };
	SDL_Rect **modes;
	int numModes;
	int i;

	modes = SDL_ListModes( videoInfo->vfmt, SDL_OPENGL | SDL_FULLSCREEN );

	if( !modes )
	{
		ri.Printf( PRINT_WARNING, "Can't get list of available modes\n" );
		return;
	}

	if( modes == (SDL_Rect **)-1 )
	{
		ri.Printf( PRINT_ALL, "Display supports any resolution\n" );
		return; // can set any resolution
	}

	for( numModes = 0; modes[ numModes ]; numModes++ );

	if( numModes > 1 )
		qsort( modes, numModes, sizeof( SDL_Rect* ), GLimp_CompareModes );

	for( i = 0; i < numModes; i++ )
	{
		const char *newModeString = va( "%ux%u ", modes[ i ]->w, modes[ i ]->h );

		if( strlen( newModeString ) < (int)sizeof( buf ) - strlen( buf ) )
			Q_strcat( buf, sizeof( buf ), newModeString );
		else
			ri.Printf( PRINT_WARNING, "Skipping mode %ux%x, buffer too small\n", modes[i]->w, modes[i]->h );
	}

	if( *buf )
	{
		buf[ strlen( buf ) - 1 ] = 0;
		ri.Printf( PRINT_ALL, "Available modes: '%s'\n", buf );
		ri.Cvar_Set( "r_availableModes", buf );
	}
}

/*
===============
GLimp_SetMode
===============
*/
static int GLimp_SetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	const char*   glstring;
	int sdlcolorbits;
	int colorbits, depthbits, stencilbits;
	int tcolorbits, tdepthbits, tstencilbits;
	int samples;
	int i = 0;
	SDL_Surface *vidscreen = NULL;
	Uint32 flags = SDL_OPENGL;

	ri.Printf( PRINT_ALL, "Initializing OpenGL display\n");

	if ( r_allowResize->integer )
		flags |= SDL_RESIZABLE;

	if( videoInfo == NULL )
	{
		static SDL_VideoInfo sVideoInfo;
		static SDL_PixelFormat sPixelFormat;

		videoInfo = SDL_GetVideoInfo( );

		// Take a copy of the videoInfo
		Com_Memcpy( &sPixelFormat, videoInfo->vfmt, sizeof( SDL_PixelFormat ) );
		sPixelFormat.palette = NULL; // Should already be the case
		Com_Memcpy( &sVideoInfo, videoInfo, sizeof( SDL_VideoInfo ) );
		sVideoInfo.vfmt = &sPixelFormat;
		videoInfo = &sVideoInfo;

		if( videoInfo->current_h > 0 )
		{
			// Guess the display aspect ratio through the desktop resolution
			// by assuming (relatively safely) that it is set at or close to
			// the display's native aspect ratio
			glGlobals.displayAspect = (float)videoInfo->current_w / (float)videoInfo->current_h;

			ri.Printf( PRINT_ALL, "Estimated display aspect: %.3f\n", glGlobals.displayAspect );
		}
		else
		{
			ri.Printf( PRINT_ALL,
					"Cannot estimate display aspect, assuming 1.333\n" );
		}
	}

	ri.Printf (PRINT_ALL, "...setting mode %d:", mode );

	if ( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )
	{
		ri.Printf( PRINT_ALL, " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	ri.Printf( PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight);

	if (fullscreen)
	{
		flags |= SDL_FULLSCREEN;
		glConfig.isFullscreen = qtrue;
	}
	else
	{
		if (noborder)
			flags |= SDL_NOFRAME;

		glConfig.isFullscreen = qfalse;
	}

	colorbits = r_colorbits->value;
	if ((!colorbits) || (colorbits >= 32))
		colorbits = 24;

	if (!r_depthbits->value)
		depthbits = 24;
	else
		depthbits = r_depthbits->value;
	stencilbits = r_stencilbits->value;
	samples = r_ext_multisample->value;

	for (i = 0; i < 16; i++)
	{
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
				case 2 :
					if (colorbits == 24)
						colorbits = 16;
					break;
				case 1 :
					if (depthbits == 24)
						depthbits = 16;
					else if (depthbits == 16)
						depthbits = 8;
				case 3 :
					if (stencilbits == 24)
						stencilbits = 16;
					else if (stencilbits == 16)
						stencilbits = 8;
			}
		}

		tcolorbits = colorbits;
		tdepthbits = depthbits;
		tstencilbits = stencilbits;

		if ((i % 4) == 3)
		{ // reduce colorbits
			if (tcolorbits == 24)
				tcolorbits = 16;
		}

		if ((i % 4) == 2)
		{ // reduce depthbits
			if (tdepthbits == 24)
				tdepthbits = 16;
			else if (tdepthbits == 16)
				tdepthbits = 8;
		}

		if ((i % 4) == 1)
		{ // reduce stencilbits
			if (tstencilbits == 24)
				tstencilbits = 16;
			else if (tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		sdlcolorbits = 4;
		if (tcolorbits == 24)
			sdlcolorbits = 8;

#ifdef __sgi /* Fix for SGIs grabbing too many bits of color */
		if (sdlcolorbits == 4)
			sdlcolorbits = 0; /* Use minimum size for 16-bit color */

		/* Need alpha or else SGIs choose 36+ bit RGB mode */
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 1);
#endif

		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, tdepthbits );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, tstencilbits );

		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, samples );

		if(r_stereoEnabled->integer)
		{
			glConfig.stereoEnabled = qtrue;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
		}
		else
		{
			glConfig.stereoEnabled = qfalse;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
		}
		
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

#if 0 // See http://bugzilla.icculus.org/show_bug.cgi?id=3526
		// If not allowing software GL, demand accelerated
		if( !r_allowSoftwareGL->integer )
		{
			if( SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 ) < 0 )
			{
				ri.Printf( PRINT_ALL, "Unable to guarantee accelerated "
						"visual with libSDL < 1.2.10\n" );
			}
		}
#endif

		if( SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, r_swapInterval->integer ) < 0 )
			ri.Printf( PRINT_ALL, "r_swapInterval requires libSDL >= 1.2.10\n" );

#ifdef USE_ICON
		{
			SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(
					(void *)CLIENT_WINDOW_ICON.pixel_data,
					CLIENT_WINDOW_ICON.width,
					CLIENT_WINDOW_ICON.height,
					CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
					CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
					0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
					0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
					);

			SDL_WM_SetIcon( icon, NULL );
			SDL_FreeSurface( icon );
		}
#endif

		SDL_WM_SetCaption(CLIENT_WINDOW_TITLE, CLIENT_WINDOW_MIN_TITLE);
		SDL_ShowCursor(0);

		if (!(vidscreen = SDL_SetVideoMode(glConfig.vidWidth, glConfig.vidHeight, colorbits, flags)))
		{
			ri.Printf( PRINT_DEVELOPER, "SDL_SetVideoMode failed: %s\n", SDL_GetError( ) );
			continue;
		}

		GLimp_GetCurrentContext( &initial_context );

		if( r_ext_debug_output->integer ) {
			GLimp_CreateSharedContext( &initial_context,
						   qtrue, qfalse,
						   &backend_context );
		} else {
			backend_context = initial_context;
		}

#ifdef SMP
		if( r_smp->integer ) {
			GLimp_CreateSharedContext( &initial_context,
						   !!r_ext_debug_output->integer,
						   qtrue,
						   &frontend_context );
		} else
#endif
			frontend_context = backend_context;

		GLimp_SetCurrentContext( &frontend_context );

		ri.Printf( PRINT_DEVELOPER, "Using %d/%d/%d Color bits, %d depth, %d stencil display.\n",
				sdlcolorbits, sdlcolorbits, sdlcolorbits, tdepthbits, tstencilbits);

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;
		break;
	}

	GLimp_DetectAvailableModes();

	if (!vidscreen)
	{
		ri.Printf( PRINT_ALL, "Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	screen = vidscreen;

	glstring = (char *) qglGetString (GL_RENDERER);
	ri.Printf( PRINT_ALL, "GL_RENDERER: %s\n", glstring );

	return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	rserr_t err;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		char driverName[ 64 ];

		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			ri.Printf( PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n",
					SDL_GetError());
			return qfalse;
		}

		SDL_VideoDriverName( driverName, sizeof( driverName ) - 1 );
		ri.Printf( PRINT_ALL, "SDL using driver \"%s\"\n", driverName );
		ri.Cvar_Set( "r_sdlDriver", driverName );
	}

	if (fullscreen && ri.Cvar_VariableIntegerValue( "in_nograb" ) )
	{
		ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}
	
	err = GLimp_SetMode(mode, fullscreen, noborder);

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			ri.Printf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			ri.Printf( PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode );
			return qfalse;
		default:
			break;
	}

	return qtrue;
}

static qboolean GLimp_HaveExtension(const char *ext)
{
	const char *ptr = Q_stristr( glConfig.extensions_string, ext );
	if (ptr == NULL)
		return qfalse;
	ptr += strlen(ext);
	return ((*ptr == ' ') || (*ptr == '\0'));  // verify it's complete string.
}


/*
===============
GLimp_InitExtensions
===============
*/
static void GLimp_InitExtensions( int GLversion )
{
#ifndef __GNUC__
#define qglGetProc2(var,proc) q##var = (void *)SDL_GL_GetProcAddress( #proc )
#else
#define qglGetProc2(var,proc) q##var = (typeof(q##var))SDL_GL_GetProcAddress( #proc )
#endif
#define qglGetProc(name,ext) qglGetProc2(name##ext,name)

	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n" );
		GLversion = 0x0000;
	} else {
		ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );
	}

	// GL_EXT_draw_range_elements, mandatory since OpenGL 1.2
	if ( GLversion >= 0x0102 ) {
		qglGetProc(glDrawRangeElements, EXT );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_EXT_draw_range_elements" ) ) {
		qglGetProc(glDrawRangeElementsEXT, );
	} else {
		qglDrawRangeElementsEXT = NULL;
	}

	glConfig.textureCompression = TC_NONE;

	// GL_EXT_texture_compression_s3tc
	if ( GLversion &&
	     GLimp_HaveExtension( "GL_ARB_texture_compression" ) &&
	     GLimp_HaveExtension( "GL_EXT_texture_compression_s3tc" ) )
	{
		if ( r_ext_compressed_textures->value )
		{
			glConfig.textureCompression = TC_S3TC_ARB;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n" );
	}

	// GL_S3_s3tc ... legacy extension before GL_EXT_texture_compression_s3tc.
	if (glConfig.textureCompression == TC_NONE)
	{
		if ( GLversion &&
		     GLimp_HaveExtension( "GL_S3_s3tc" ) )
		{
			if ( r_ext_compressed_textures->value )
			{
				glConfig.textureCompression = TC_S3TC;
				ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
			}
			else
			{
				ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
		}
	}


	// GL_EXT_texture_env_add, mandatory since OpenGL 1.3
	if ( !r_ext_texture_env_add->integer ) {
		glConfig.textureEnvAddAvailable = qfalse;
		ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
	} else if ( GLversion >= 0x0103 ) {
		glConfig.textureEnvAddAvailable = qtrue;
		ri.Printf( PRINT_ALL, "...using GL_texture_env_add\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "EXT_texture_env_add" ) ) {
		glConfig.textureEnvAddAvailable = qtrue;
		ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
	}
	else
	{
		glConfig.textureEnvAddAvailable = qfalse;
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_ARB_multitexture, mandatory since OpenGL 1.3
	if ( !r_ext_multitexture->value ) {
		qglMultiTexCoord2fARB = NULL;
		qglMultiTexCoord4fvARB = NULL;
		qglActiveTextureARB = NULL;
		qglClientActiveTextureARB = NULL;
		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
	} else if ( GLversion >= 0x0103 ) {
		qglGetProc(glMultiTexCoord2f, ARB);
		qglGetProc(glMultiTexCoord4fv, ARB);
		qglGetProc(glActiveTexture, ARB);
		qglGetProc(glClientActiveTexture, ARB);
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_multitexture" ) ) {
		qglGetProc(glMultiTexCoord2fARB, );
		qglGetProc(glMultiTexCoord4fvARB, );
		qglGetProc(glActiveTextureARB, );
		qglGetProc(glClientActiveTextureARB, );
	} else {
		qglMultiTexCoord2fARB = NULL;
		qglMultiTexCoord4fvARB = NULL;
		qglActiveTextureARB = NULL;
		qglClientActiveTextureARB = NULL;
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}
	if ( qglActiveTextureARB )
	{
		qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glGlobals.maxTextureUnits );
		glConfig.numTextureUnits = (int) glGlobals.maxTextureUnits;
		if ( glConfig.numTextureUnits > NUM_TEXTURE_BUNDLES )
			glConfig.numTextureUnits = NUM_TEXTURE_BUNDLES;
		if ( r_ext_multitexture->integer > 1 &&
		     glConfig.numTextureUnits > r_ext_multitexture->integer )
			glConfig.numTextureUnits = r_ext_multitexture->integer;
		if ( glConfig.numTextureUnits > 1 )
		{
			ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture (%d of %d units)\n", glConfig.numTextureUnits, glGlobals.maxTextureUnits );
		}
		else
		{
			qglMultiTexCoord2fARB = NULL;
			qglActiveTextureARB = NULL;
			qglClientActiveTextureARB = NULL;
			ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
		}
	}

	// GL_ARB_vertex_buffer_object, mandatory since OpenGL 1.5
	if ( !r_ext_vertex_buffer_object->integer ) {
		qglBindBufferARB = NULL;
		qglDeleteBuffersARB = NULL;
		qglGenBuffersARB = NULL;
		qglIsBufferARB = NULL;
		qglBufferDataARB = NULL;
		qglBufferSubDataARB = NULL;
		qglGetBufferSubDataARB = NULL;
		qglMapBufferARB = NULL;
		qglUnmapBufferARB = NULL;
		qglGetBufferParameterivARB = NULL;
		qglGetBufferPointervARB = NULL;
		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_vertex_buffer_object\n" );
	} else if ( GLversion >= 0x0105 ) {
		qglGetProc(glBindBuffer, ARB);
		qglGetProc(glDeleteBuffers, ARB);
		qglGetProc(glGenBuffers, ARB);
		qglGetProc(glIsBuffer, ARB);
		qglGetProc(glBufferData, ARB);
		qglGetProc(glBufferSubData, ARB);
		qglGetProc(glGetBufferSubData, ARB);
		qglGetProc(glMapBuffer, ARB);
		qglGetProc(glUnmapBuffer, ARB);
		qglGetProc(glGetBufferParameteriv, ARB);
		qglGetProc(glGetBufferPointerv, ARB);
		ri.Printf( PRINT_ALL, "...using GL_vertex_buffer_object\n");
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_vertex_buffer_object" ) ) {
		qglGetProc(glBindBufferARB, );
		qglGetProc(glDeleteBuffersARB, );
		qglGetProc(glGenBuffersARB, );
		qglGetProc(glIsBufferARB, );
		qglGetProc(glBufferDataARB, );
		qglGetProc(glBufferSubDataARB, );
		qglGetProc(glGetBufferSubDataARB, );
		qglGetProc(glMapBufferARB, );
		qglGetProc(glUnmapBufferARB, );
		qglGetProc(glGetBufferParameterivARB, );
		qglGetProc(glGetBufferPointervARB, );
		ri.Printf( PRINT_ALL, "...using GL_ARB_vertex_buffer_object\n" );
	} else {
		qglBindBufferARB = NULL;
		qglDeleteBuffersARB = NULL;
		qglGenBuffersARB = NULL;
		qglIsBufferARB = NULL;
		qglBufferDataARB = NULL;
		qglBufferSubDataARB = NULL;
		qglGetBufferSubDataARB = NULL;
		qglMapBufferARB = NULL;
		qglUnmapBufferARB = NULL;
		qglGetBufferParameterivARB = NULL;
		qglGetBufferPointervARB = NULL;
		ri.Printf( PRINT_ALL, "...GL_ARB_vertex_buffer_object not found\n" );
	}

	// GL_ARB_map_buffer_range, mandatory since OpenGL 3.0
	if ( !r_ext_map_buffer_range->integer ) {
		qglMapBufferRange = NULL;
		qglFlushMappedBufferRange = NULL;
		ri.Printf( PRINT_DEVELOPER, "...ignoring GL_EXT_map_buffer_range\n" );
	} else if ( GLversion >= 0x0300 ) {
		qglGetProc(glMapBufferRange, );
		qglGetProc(glFlushMappedBufferRange, );
		ri.Printf( PRINT_DEVELOPER, "...using GL_map_buffer_range\n");
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_map_buffer_range" ) ) {
		qglGetProc(glMapBufferRange, );
		qglGetProc(glFlushMappedBufferRange, );
		ri.Printf( PRINT_DEVELOPER, "...using GL_ARB_map_buffer_range\n" );
	} else {
		qglMapBufferRange = NULL;
		qglFlushMappedBufferRange = NULL;
		ri.Printf( PRINT_DEVELOPER, "...GL_ARB_map_buffer_range not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	if ( !r_ext_compiled_vertex_array->value ) {
		qglLockArraysEXT = NULL;
		qglUnlockArraysEXT = NULL;
		ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_EXT_compiled_vertex_array" ) ) {
		qglGetProc(glLockArraysEXT, );
		qglGetProc(glUnlockArraysEXT, );
		ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
	} else {
		qglLockArraysEXT = NULL;
		qglUnlockArraysEXT = NULL;
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}

	// GL_EXT_texture_filter_anisotropic
	if ( !r_ext_texture_filter_anisotropic->integer ) {
		glGlobals.textureFilterAnisotropic = qfalse;
		ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_EXT_texture_filter_anisotropic" ) ) {
		qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&glGlobals.maxAnisotropy );
		if ( glGlobals.maxAnisotropy <= 0 ) {
			ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not properly supported!\n" );
			glGlobals.maxAnisotropy = 0;
		}
		else
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", glGlobals.maxAnisotropy );
			glGlobals.textureFilterAnisotropic = qtrue;
		}
	} else {
		glGlobals.textureFilterAnisotropic = qfalse;
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
	}

	// GLSL support, mandatory since OpenGL 2.0
	if ( !r_ext_vertex_shader->integer ) {
		qglDeleteShader = NULL;
		qglDeleteProgram = NULL;
		qglDetachShader = NULL;
		qglCreateShader = NULL;
		qglShaderSource = NULL;
		qglCompileShader = NULL;
		qglCreateProgram = NULL;
		qglAttachShader = NULL;
		qglLinkProgram = NULL;
		qglUseProgram = NULL;
		qglValidateProgram = NULL;
		qglUniform1f = NULL;
		qglUniform2f = NULL;
		qglUniform3f = NULL;
		qglUniform4f = NULL;
		qglUniform1i = NULL;
		qglUniform2i = NULL;
		qglUniform3i = NULL;
		qglUniform4i = NULL;
		qglUniform1fv = NULL;
		qglUniform2fv = NULL;
		qglUniform3fv = NULL;
		qglUniform4fv = NULL;
		qglUniform1iv = NULL;
		qglUniform2iv = NULL;
		qglUniform3iv = NULL;
		qglUniform4iv = NULL;
		qglUniformMatrix2fv = NULL;
		qglUniformMatrix3fv = NULL;
		qglUniformMatrix4fv = NULL;
		qglGetShaderiv = NULL;
		qglGetProgramiv = NULL;
		qglGetShaderInfoLog = NULL;
		qglGetProgramInfoLog = NULL;
		qglGetAttachedShaders = NULL;
		qglGetUniformLocation = NULL;
		qglGetActiveUniform = NULL;
		qglGetUniformfv = NULL;
		qglGetUniformiv = NULL;
		qglGetShaderSource = NULL;
		
		qglVertexAttrib1fARB = NULL;
		qglVertexAttrib1sARB = NULL;
		qglVertexAttrib1dARB = NULL;
		qglVertexAttrib2fARB = NULL;
		qglVertexAttrib2sARB = NULL;
		qglVertexAttrib2dARB = NULL;
		qglVertexAttrib3fARB = NULL;
		qglVertexAttrib3sARB = NULL;
		qglVertexAttrib3dARB = NULL;
		qglVertexAttrib4fARB = NULL;
		qglVertexAttrib4sARB = NULL;
		qglVertexAttrib4dARB = NULL;
		qglVertexAttrib4NubARB = NULL;
		qglVertexAttrib1fvARB = NULL;
		qglVertexAttrib1svARB = NULL;
		qglVertexAttrib1dvARB = NULL;
		qglVertexAttrib2fvARB = NULL;
		qglVertexAttrib2svARB = NULL;
		qglVertexAttrib2dvARB = NULL;
		qglVertexAttrib3fvARB = NULL;
		qglVertexAttrib3svARB = NULL;
		qglVertexAttrib3dvARB = NULL;
		qglVertexAttrib4fvARB = NULL;
		qglVertexAttrib4svARB = NULL;
		qglVertexAttrib4dvARB = NULL;
		qglVertexAttrib4ivARB = NULL;
		qglVertexAttrib4bvARB = NULL;
		qglVertexAttrib4ubvARB = NULL;
		qglVertexAttrib4usvARB = NULL;
		qglVertexAttrib4uivARB = NULL;
		qglVertexAttrib4NbvARB = NULL;
		qglVertexAttrib4NsvARB = NULL;
		qglVertexAttrib4NivARB = NULL;
		qglVertexAttrib4NubvARB = NULL;
		qglVertexAttrib4NusvARB = NULL;
		qglVertexAttrib4NuivARB = NULL;
		qglVertexAttribPointerARB = NULL;
		qglEnableVertexAttribArrayARB = NULL;
		qglDisableVertexAttribArrayARB = NULL;
		qglBindAttribLocationARB = NULL;
		qglGetActiveAttribARB = NULL;
		qglGetAttribLocationARB = NULL;
		qglGetVertexAttribdvARB = NULL;
		qglGetVertexAttribfvARB = NULL;
		qglGetVertexAttribivARB = NULL;
		qglGetVertexAttribPointervARB = NULL;
		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_vertex_shader\n" );
	} else if ( GLversion >= 0x0200 ) {
		qglGetProc(glDeleteShader, );
		qglGetProc(glDeleteProgram, );
		qglGetProc(glDetachShader, );
		qglGetProc(glCreateShader, );
		qglGetProc(glShaderSource, );
		qglGetProc(glCompileShader, );
		qglGetProc(glCreateProgram, );
		qglGetProc(glAttachShader, );
		qglGetProc(glLinkProgram, );
		qglGetProc(glUseProgram, );
		qglGetProc(glValidateProgram, );
		qglGetProc(glUniform1f, );
		qglGetProc(glUniform2f, );
		qglGetProc(glUniform3f, );
		qglGetProc(glUniform4f, );
		qglGetProc(glUniform1i, );
		qglGetProc(glUniform2i, );
		qglGetProc(glUniform3i, );
		qglGetProc(glUniform4i, );
		qglGetProc(glUniform1fv, );
		qglGetProc(glUniform2fv, );
		qglGetProc(glUniform3fv, );
		qglGetProc(glUniform4fv, );
		qglGetProc(glUniform1iv, );
		qglGetProc(glUniform2iv, );
		qglGetProc(glUniform3iv, );
		qglGetProc(glUniform4iv, );
		qglGetProc(glUniformMatrix2fv, );
		qglGetProc(glUniformMatrix3fv, );
		qglGetProc(glUniformMatrix4fv, );
		qglGetProc(glGetShaderiv, );
		qglGetProc(glGetProgramiv, );
		qglGetProc(glGetShaderInfoLog, );
		qglGetProc(glGetProgramInfoLog, );
		qglGetProc(glGetAttachedShaders, );
		qglGetProc(glGetUniformLocation, );
		qglGetProc(glGetActiveUniform, );
		qglGetProc(glGetUniformfv, );
		qglGetProc(glGetUniformiv, );
		qglGetProc(glGetShaderSource, );
		
		qglGetProc(glVertexAttrib1f, ARB);
		qglGetProc(glVertexAttrib1s, ARB);
		qglGetProc(glVertexAttrib1d, ARB);
		qglGetProc(glVertexAttrib2f, ARB);
		qglGetProc(glVertexAttrib2s, ARB);
		qglGetProc(glVertexAttrib2d, ARB);
		qglGetProc(glVertexAttrib3f, ARB);
		qglGetProc(glVertexAttrib3s, ARB);
		qglGetProc(glVertexAttrib3d, ARB);
		qglGetProc(glVertexAttrib4f, ARB);
		qglGetProc(glVertexAttrib4s, ARB);
		qglGetProc(glVertexAttrib4d, ARB);
		qglGetProc(glVertexAttrib4Nub, ARB);
		qglGetProc(glVertexAttrib1fv, ARB);
		qglGetProc(glVertexAttrib1sv, ARB);
		qglGetProc(glVertexAttrib1dv, ARB);
		qglGetProc(glVertexAttrib2fv, ARB);
		qglGetProc(glVertexAttrib2sv, ARB);
		qglGetProc(glVertexAttrib2dv, ARB);
		qglGetProc(glVertexAttrib3fv, ARB);
		qglGetProc(glVertexAttrib3sv, ARB);
		qglGetProc(glVertexAttrib3dv, ARB);
		qglGetProc(glVertexAttrib4fv, ARB);
		qglGetProc(glVertexAttrib4sv, ARB);
		qglGetProc(glVertexAttrib4dv, ARB);
		qglGetProc(glVertexAttrib4iv, ARB);
		qglGetProc(glVertexAttrib4bv, ARB);
		qglGetProc(glVertexAttrib4ubv, ARB);
		qglGetProc(glVertexAttrib4usv, ARB);
		qglGetProc(glVertexAttrib4uiv, ARB);
		qglGetProc(glVertexAttrib4Nbv, ARB);
		qglGetProc(glVertexAttrib4Nsv, ARB);
		qglGetProc(glVertexAttrib4Niv, ARB);
		qglGetProc(glVertexAttrib4Nubv, ARB);
		qglGetProc(glVertexAttrib4Nusv, ARB);
		qglGetProc(glVertexAttrib4Nuiv, ARB);
		qglGetProc(glVertexAttribPointer, ARB);
		qglGetProc(glEnableVertexAttribArray, ARB);
		qglGetProc(glDisableVertexAttribArray, ARB);
		qglGetProc(glBindAttribLocation, ARB);
		qglGetProc(glGetActiveAttrib, ARB);
		qglGetProc(glGetAttribLocation, ARB);
		qglGetProc(glGetVertexAttribdv, ARB);
		qglGetProc(glGetVertexAttribfv, ARB);
		qglGetProc(glGetVertexAttribiv, ARB);
		qglGetProc(glGetVertexAttribPointerv, ARB);

		ri.Printf( PRINT_ALL, "...using GL_vertex_shader\n" );
		
	} else if ( GLversion &&
	     GLimp_HaveExtension( "GL_ARB_shader_objects" ) &&
	     GLimp_HaveExtension( "GL_ARB_fragment_shader" ) &&
	     GLimp_HaveExtension( "GL_ARB_vertex_shader" ) &&
	     GLimp_HaveExtension( "GL_ARB_shading_language_100" ) )
	{
		// many functions have been renamed from the ARB ext to GL 2.0
		qglGetProc2(glDeleteShader, glDeleteObjectARB);
		qglGetProc2(glDeleteProgram, glDeleteObjectARB);
		qglGetProc2(glDetachShader, glDetachObjectARB);
		qglGetProc2(glCreateShader, glCreateShaderObjectARB);
		qglGetProc2(glShaderSource, glShaderSourceARB);
		qglGetProc2(glCompileShader, glCompileShaderARB);
		qglGetProc2(glCreateProgram, glCreateProgramObjectARB);
		qglGetProc2(glAttachShader, glAttachObjectARB);
		qglGetProc2(glLinkProgram, glLinkProgramARB);
		qglGetProc2(glUseProgram, glUseProgramObjectARB);
		qglGetProc2(glValidateProgram, glValidateProgramARB);
		qglGetProc2(glUniform1f, glUniform1fARB);
		qglGetProc2(glUniform2f, glUniform2fARB);
		qglGetProc2(glUniform3f, glUniform3fARB);
		qglGetProc2(glUniform4f, glUniform4fARB);
		qglGetProc2(glUniform1i, glUniform1iARB);
		qglGetProc2(glUniform2i, glUniform2iARB);
		qglGetProc2(glUniform3i, glUniform3iARB);
		qglGetProc2(glUniform4i, glUniform4iARB);
		qglGetProc2(glUniform1fv, glUniform1fvARB);
		qglGetProc2(glUniform2fv, glUniform2fvARB);
		qglGetProc2(glUniform3fv, glUniform3fvARB);
		qglGetProc2(glUniform4fv, glUniform4fvARB);
		qglGetProc2(glUniform1iv, glUniform1ivARB);
		qglGetProc2(glUniform2iv, glUniform2ivARB);
		qglGetProc2(glUniform3iv, glUniform3ivARB);
		qglGetProc2(glUniform4iv, glUniform4ivARB);
		qglGetProc2(glUniform2fv, glUniformMatrix2fvARB);
		qglGetProc2(glUniform3fv, glUniformMatrix3fvARB);
		qglGetProc2(glUniform4fv, glUniformMatrix4fvARB);
		qglGetProc2(glGetShaderiv, glGetObjectParameterivARB);
		qglGetProc2(glGetProgramiv, glGetObjectParameterivARB);
		qglGetProc2(glGetShaderInfoLog, glGetInfoLogARB);
		qglGetProc2(glGetProgramInfoLog, glGetInfoLogARB);
		qglGetProc2(glGetAttachedShaders, glGetAttachedObjectsARB);
		qglGetProc2(glGetUniformLocation, glGetUniformLocationARB);
		qglGetProc2(glGetActiveUniform, glGetActiveUniformARB);
		qglGetProc2(glGetUniformfv, glGetUniformfvARB);
		qglGetProc2(glGetUniformiv, glGetUniformivARB);
		qglGetProc2(glGetShaderSource, glGetShaderSourceARB);
		
		qglGetProc(glVertexAttrib1fARB, );
		qglGetProc(glVertexAttrib1sARB, );
		qglGetProc(glVertexAttrib1dARB, );
		qglGetProc(glVertexAttrib2fARB, );
		qglGetProc(glVertexAttrib2sARB, );
		qglGetProc(glVertexAttrib2dARB, );
		qglGetProc(glVertexAttrib3fARB, );
		qglGetProc(glVertexAttrib3sARB, );
		qglGetProc(glVertexAttrib3dARB, );
		qglGetProc(glVertexAttrib4fARB, );
		qglGetProc(glVertexAttrib4sARB, );
		qglGetProc(glVertexAttrib4dARB, );
		qglGetProc(glVertexAttrib4NubARB, );
		qglGetProc(glVertexAttrib1fvARB, );
		qglGetProc(glVertexAttrib1svARB, );
		qglGetProc(glVertexAttrib1dvARB, );
		qglGetProc(glVertexAttrib2fvARB, );
		qglGetProc(glVertexAttrib2svARB, );
		qglGetProc(glVertexAttrib2dvARB, );
		qglGetProc(glVertexAttrib3fvARB, );
		qglGetProc(glVertexAttrib3svARB, );
		qglGetProc(glVertexAttrib3dvARB, );
		qglGetProc(glVertexAttrib4fvARB, );
		qglGetProc(glVertexAttrib4svARB, );
		qglGetProc(glVertexAttrib4dvARB, );
		qglGetProc(glVertexAttrib4ivARB, );
		qglGetProc(glVertexAttrib4bvARB, );
		qglGetProc(glVertexAttrib4ubvARB, );
		qglGetProc(glVertexAttrib4usvARB, );
		qglGetProc(glVertexAttrib4uivARB, );
		qglGetProc(glVertexAttrib4NbvARB, );
		qglGetProc(glVertexAttrib4NsvARB, );
		qglGetProc(glVertexAttrib4NivARB, );
		qglGetProc(glVertexAttrib4NubvARB, );
		qglGetProc(glVertexAttrib4NusvARB, );
		qglGetProc(glVertexAttrib4NuivARB, );
		qglGetProc(glVertexAttribPointerARB, );
		qglGetProc(glEnableVertexAttribArrayARB, );
		qglGetProc(glDisableVertexAttribArrayARB, );
		qglGetProc(glBindAttribLocationARB, );
		qglGetProc(glGetActiveAttribARB, );
		qglGetProc(glGetAttribLocationARB, );
		qglGetProc(glGetVertexAttribdvARB, );
		qglGetProc(glGetVertexAttribfvARB, );
		qglGetProc(glGetVertexAttribivARB, );
		qglGetProc(glGetVertexAttribPointervARB, );

		ri.Printf( PRINT_ALL, "...using GL_ARB_vertex_shader\n" );
	}
	else
	{
		qglDeleteShader = NULL;
		qglDeleteProgram = NULL;
		qglDetachShader = NULL;
		qglCreateShader = NULL;
		qglShaderSource = NULL;
		qglCompileShader = NULL;
		qglCreateProgram = NULL;
		qglAttachShader = NULL;
		qglLinkProgram = NULL;
		qglUseProgram = NULL;
		qglValidateProgram = NULL;
		qglUniform1f = NULL;
		qglUniform2f = NULL;
		qglUniform3f = NULL;
		qglUniform4f = NULL;
		qglUniform1i = NULL;
		qglUniform2i = NULL;
		qglUniform3i = NULL;
		qglUniform4i = NULL;
		qglUniform1fv = NULL;
		qglUniform2fv = NULL;
		qglUniform3fv = NULL;
		qglUniform4fv = NULL;
		qglUniform1iv = NULL;
		qglUniform2iv = NULL;
		qglUniform3iv = NULL;
		qglUniform4iv = NULL;
		qglUniformMatrix2fv = NULL;
		qglUniformMatrix3fv = NULL;
		qglUniformMatrix4fv = NULL;
		qglGetShaderiv = NULL;
		qglGetProgramiv = NULL;
		qglGetShaderInfoLog = NULL;
		qglGetProgramInfoLog = NULL;
		qglGetAttachedShaders = NULL;
		qglGetUniformLocation = NULL;
		qglGetActiveUniform = NULL;
		qglGetUniformfv = NULL;
		qglGetUniformiv = NULL;
		qglGetShaderSource = NULL;
		
		qglVertexAttrib1fARB = NULL;
		qglVertexAttrib1sARB = NULL;
		qglVertexAttrib1dARB = NULL;
		qglVertexAttrib2fARB = NULL;
		qglVertexAttrib2sARB = NULL;
		qglVertexAttrib2dARB = NULL;
		qglVertexAttrib3fARB = NULL;
		qglVertexAttrib3sARB = NULL;
		qglVertexAttrib3dARB = NULL;
		qglVertexAttrib4fARB = NULL;
		qglVertexAttrib4sARB = NULL;
		qglVertexAttrib4dARB = NULL;
		qglVertexAttrib4NubARB = NULL;
		qglVertexAttrib1fvARB = NULL;
		qglVertexAttrib1svARB = NULL;
		qglVertexAttrib1dvARB = NULL;
		qglVertexAttrib2fvARB = NULL;
		qglVertexAttrib2svARB = NULL;
		qglVertexAttrib2dvARB = NULL;
		qglVertexAttrib3fvARB = NULL;
		qglVertexAttrib3svARB = NULL;
		qglVertexAttrib3dvARB = NULL;
		qglVertexAttrib4fvARB = NULL;
		qglVertexAttrib4svARB = NULL;
		qglVertexAttrib4dvARB = NULL;
		qglVertexAttrib4ivARB = NULL;
		qglVertexAttrib4bvARB = NULL;
		qglVertexAttrib4ubvARB = NULL;
		qglVertexAttrib4usvARB = NULL;
		qglVertexAttrib4uivARB = NULL;
		qglVertexAttrib4NbvARB = NULL;
		qglVertexAttrib4NsvARB = NULL;
		qglVertexAttrib4NivARB = NULL;
		qglVertexAttrib4NubvARB = NULL;
		qglVertexAttrib4NusvARB = NULL;
		qglVertexAttrib4NuivARB = NULL;
		qglVertexAttribPointerARB = NULL;
		qglEnableVertexAttribArrayARB = NULL;
		qglDisableVertexAttribArrayARB = NULL;
		qglBindAttribLocationARB = NULL;
		qglGetActiveAttribARB = NULL;
		qglGetAttribLocationARB = NULL;
		qglGetVertexAttribdvARB = NULL;
		qglGetVertexAttribfvARB = NULL;
		qglGetVertexAttribivARB = NULL;
		qglGetVertexAttribPointervARB = NULL;

		ri.Printf( PRINT_ALL, "...GL_ARB_vertex_shader not found\n" );
	}

	if ( qglCreateShader ) {
		// check that fragment shaders may access enough texture image units
		// to render a whole shader in one pass
		qglGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &glGlobals.maxTextureImageUnits );
		qglGetIntegerv( GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB, &glGlobals.maxVertexTextureImageUnits );
		if ( glGlobals.maxTextureImageUnits < MAX_SHADER_STAGES + 4 ) {
			qglDeleteShader = NULL;
			qglDeleteProgram = NULL;
			qglDetachShader = NULL;
			qglCreateShader = NULL;
			qglShaderSource = NULL;
			qglCompileShader = NULL;
			qglCreateProgram = NULL;
			qglAttachShader = NULL;
			qglLinkProgram = NULL;
			qglUseProgram = NULL;
			qglValidateProgram = NULL;
			qglUniform1f = NULL;
			qglUniform2f = NULL;
			qglUniform3f = NULL;
			qglUniform4f = NULL;
			qglUniform1i = NULL;
			qglUniform2i = NULL;
			qglUniform3i = NULL;
			qglUniform4i = NULL;
			qglUniform1fv = NULL;
			qglUniform2fv = NULL;
			qglUniform3fv = NULL;
			qglUniform4fv = NULL;
			qglUniform1iv = NULL;
			qglUniform2iv = NULL;
			qglUniform3iv = NULL;
			qglUniform4iv = NULL;
			qglUniformMatrix2fv = NULL;
			qglUniformMatrix3fv = NULL;
			qglUniformMatrix4fv = NULL;
			qglGetShaderiv = NULL;
			qglGetProgramiv = NULL;
			qglGetShaderInfoLog = NULL;
			qglGetProgramInfoLog = NULL;
			qglGetAttachedShaders = NULL;
			qglGetUniformLocation = NULL;
			qglGetActiveUniform = NULL;
			qglGetUniformfv = NULL;
			qglGetUniformiv = NULL;
			qglGetShaderSource = NULL;
			
			qglVertexAttrib1fARB = NULL;
			qglVertexAttrib1sARB = NULL;
			qglVertexAttrib1dARB = NULL;
			qglVertexAttrib2fARB = NULL;
			qglVertexAttrib2sARB = NULL;
			qglVertexAttrib2dARB = NULL;
			qglVertexAttrib3fARB = NULL;
			qglVertexAttrib3sARB = NULL;
			qglVertexAttrib3dARB = NULL;
			qglVertexAttrib4fARB = NULL;
			qglVertexAttrib4sARB = NULL;
			qglVertexAttrib4dARB = NULL;
			qglVertexAttrib4NubARB = NULL;
			qglVertexAttrib1fvARB = NULL;
			qglVertexAttrib1svARB = NULL;
			qglVertexAttrib1dvARB = NULL;
			qglVertexAttrib2fvARB = NULL;
			qglVertexAttrib2svARB = NULL;
			qglVertexAttrib2dvARB = NULL;
			qglVertexAttrib3fvARB = NULL;
			qglVertexAttrib3svARB = NULL;
			qglVertexAttrib3dvARB = NULL;
			qglVertexAttrib4fvARB = NULL;
			qglVertexAttrib4svARB = NULL;
			qglVertexAttrib4dvARB = NULL;
			qglVertexAttrib4ivARB = NULL;
			qglVertexAttrib4bvARB = NULL;
			qglVertexAttrib4ubvARB = NULL;
			qglVertexAttrib4usvARB = NULL;
			qglVertexAttrib4uivARB = NULL;
			qglVertexAttrib4NbvARB = NULL;
			qglVertexAttrib4NsvARB = NULL;
			qglVertexAttrib4NivARB = NULL;
			qglVertexAttrib4NubvARB = NULL;
			qglVertexAttrib4NusvARB = NULL;
			qglVertexAttrib4NuivARB = NULL;
			qglVertexAttribPointerARB = NULL;
			qglEnableVertexAttribArrayARB = NULL;
			qglDisableVertexAttribArrayARB = NULL;
			qglBindAttribLocationARB = NULL;
			qglGetActiveAttribARB = NULL;
			qglGetAttribLocationARB = NULL;
			qglGetVertexAttribdvARB = NULL;
			qglGetVertexAttribfvARB = NULL;
			qglGetVertexAttribivARB = NULL;
			qglGetVertexAttribPointervARB = NULL;
			
			ri.Printf( PRINT_ALL, "Fragment/Vertex shaders support only %d/%d texture image units - disabled\n",
				   glGlobals.maxTextureImageUnits,
				   glGlobals.maxVertexTextureImageUnits );
		}
	} else {
		glGlobals.maxTextureImageUnits = glGlobals.maxTextureUnits;
	}

	// GL_EXT_geometry_shader4, mandatory since OpenGL 3.2
	if ( !r_ext_geometry_shader->integer ) {
		qglProgramParameteriEXT = NULL;
		qglFramebufferTextureEXT = NULL;
		qglFramebufferTextureLayerEXT = NULL;
		qglFramebufferTextureFaceEXT = NULL;

		ri.Printf( PRINT_ALL, "...ignoring GL_EXT_geometry_shader4\n" );
	} else if ( GLversion >= 0x0302 ) {
		qglGetProc(glProgramParameteri, EXT);
		qglGetProc(glFramebufferTexture, EXT);
		qglGetProc(glFramebufferTextureLayer, EXT);
		qglGetProc(glFramebufferTextureFace, EXT);

		ri.Printf( PRINT_ALL, "...using GL_geometry_shader\n" );
		
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_EXT_geometry_shader4" ) )
	{
		qglGetProc(glProgramParameteriEXT, );
		qglGetProc(glFramebufferTextureEXT, );
		qglGetProc(glFramebufferTextureLayerEXT, );
		qglGetProc(glFramebufferTextureFaceEXT, );

		ri.Printf( PRINT_ALL, "...using GL_EXT_geometry_shader4\n" );
	}
	else
	{
		qglProgramParameteriEXT = NULL;
		qglFramebufferTextureEXT = NULL;
		qglFramebufferTextureLayerEXT = NULL;
		qglFramebufferTextureFaceEXT = NULL;

		ri.Printf( PRINT_ALL, "...GL_EXT_geometry_shader4 not found\n" );
	}

	// GL_ARB_texture_float, mandatory since OpenGL 3.0
	if ( !r_ext_texture_float->integer ) {
		glGlobals.floatTextures = qfalse;
		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_texture_float\n" );
	} else if ( GLversion >= 0x0300 &&
		    glGlobals.maxVertexTextureImageUnits > 0 ) {
		glGlobals.floatTextures = qtrue;
		ri.Printf( PRINT_ALL, "...using GL_texture_float\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_texture_float" ) &&
		    glGlobals.maxVertexTextureImageUnits > 0 ) {
		glGlobals.floatTextures = qtrue;
		ri.Printf( PRINT_ALL, "...using GL_ARB_texture_float\n" );
	} else {
		glGlobals.floatTextures = qfalse;
		ri.Printf( PRINT_ALL, "...GL_ARB_texture_float not found\n" );
	}

	// GL_EXT_texture_buffer_object, mandatory since OpenGL 3.0
	if( !glGlobals.floatTextures || !r_ext_texture_buffer_object->integer ) {
		qglTexBufferEXT = NULL;
		ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_buffer_object\n" );
	} else if( GLversion >= 0x0300 ) {
		qglGetProc(glTexBuffer, EXT);
		ri.Printf( PRINT_ALL, "...using GL_texture_buffer_object\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_EXT_texture_buffer_object" ) ) {
		qglGetProc(glTexBufferEXT, );
		ri.Printf( PRINT_ALL, "...using GL_EXT_texture_buffer_object\n" );
	} else {
		qglTexBufferEXT = NULL;
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_buffer_object not found\n" );
	}

	// GL_ARB_uniform_buffer_object, mandatory since OpenGL 3.1
	if( !r_ext_uniform_buffer_object->integer ) {
		qglGetUniformIndices = NULL;
		qglGetActiveUniformsiv = NULL;
		qglGetActiveUniformName = NULL;
		qglGetUniformBlockIndex = NULL;
		qglGetActiveUniformBlockiv = NULL;
		qglGetActiveUniformBlockName = NULL;
		qglBindBufferRange = NULL;
		qglBindBufferBase = NULL;
		qglGetIntegeri_v = NULL;
		qglUniformBlockBinding = NULL;
		ri.Printf( PRINT_DEVELOPER, "...ignoring GL_ARB_uniform_buffer_object\n" );
	} else if( GLversion >= 0x0301 ) {
		qglGetProc(glGetUniformIndices, );
		qglGetProc(glGetActiveUniformsiv, );
		qglGetProc(glGetActiveUniformName, );
		qglGetProc(glGetUniformBlockIndex, );
		qglGetProc(glGetActiveUniformBlockiv, );
		qglGetProc(glGetActiveUniformBlockName, );
		qglGetProc(glBindBufferRange, );
		qglGetProc(glBindBufferBase, );
		qglGetProc(glGetIntegeri_v, );
		qglGetProc(glUniformBlockBinding, );
		ri.Printf( PRINT_DEVELOPER, "...using GL_uniform_buffer_object\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_uniform_buffer_object" ) ) {
		qglGetProc(glGetUniformIndices, );
		qglGetProc(glGetActiveUniformsiv, );
		qglGetProc(glGetActiveUniformName, );
		qglGetProc(glGetUniformBlockIndex, );
		qglGetProc(glGetActiveUniformBlockiv, );
		qglGetProc(glGetActiveUniformBlockName, );
		qglGetProc(glBindBufferRange, );
		qglGetProc(glBindBufferBase, );
		qglGetProc(glGetIntegeri_v, );
		qglGetProc(glUniformBlockBinding, );
		ri.Printf( PRINT_DEVELOPER, "...using GL_ARB_uniform_buffer_object\n" );
	} else {
		qglGetUniformIndices = NULL;
		qglGetActiveUniformsiv = NULL;
		qglGetActiveUniformName = NULL;
		qglGetUniformBlockIndex = NULL;
		qglGetActiveUniformBlockiv = NULL;
		qglGetActiveUniformBlockName = NULL;
		qglBindBufferRange = NULL;
		qglBindBufferBase = NULL;
		qglGetIntegeri_v = NULL;
		qglUniformBlockBinding = NULL;
		ri.Printf( PRINT_DEVELOPER, "...GL_ARB_uniform_buffer_object not found\n" );
	}

	// GL_EXT_texture3D, mandatory since OpenGL 1.2
	if ( !r_ext_texture3D->integer ) {
		qglTexImage3DEXT = NULL;
		ri.Printf( PRINT_DEVELOPER, "...ignoring GL_EXT_texture3D\n" );
	} else if ( GLversion >= 0x0102 ) {
		qglGetProc(glTexImage3D, EXT);
		ri.Printf( PRINT_DEVELOPER, "...using GL_texture3D\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_EXT_texture3D" ) ) {
		qglGetProc(glTexImage3DEXT, );
		ri.Printf( PRINT_DEVELOPER, "...using GL_EXT_texture3D\n" );
	} else {
		qglTexImage3DEXT = NULL;
		ri.Printf( PRINT_DEVELOPER, "...GL_EXT_texture3D not found\n" );
	}

	// GL_ARB_framebuffer_object, mandatory since OpenGL 3.0
	if ( !r_ext_framebuffer_object->integer ) {
		qglIsRenderbuffer = NULL;
		qglBindRenderbuffer = NULL;
		qglDeleteRenderbuffers = NULL;
		qglGenRenderbuffers = NULL;
		qglRenderbufferStorage = NULL;
		qglRenderbufferStorageMultisample = NULL;
		qglGetRenderbufferParameteriv = NULL;
		qglIsFramebuffer = NULL;
		qglBindFramebuffer = NULL;
		qglDeleteFramebuffers = NULL;
		qglGenFramebuffers = NULL;
		qglCheckFramebufferStatus = NULL;
		qglFramebufferTexture1D = NULL;
		qglFramebufferTexture2D = NULL;
		qglFramebufferTexture3D = NULL;
		qglFramebufferTextureLayer = NULL;
		qglFramebufferRenderbuffer = NULL;
		qglGetFramebufferAttachmentParameteriv = NULL;
		qglBlitFramebuffer = NULL;
		qglGenerateMipmap = NULL;
		
		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_framebuffer_object\n" );
	} else if ( GLversion >= 0x0300 ) {
		qglGetProc(glIsRenderbuffer, );
		qglGetProc(glBindRenderbuffer, );
		qglGetProc(glDeleteRenderbuffers, );
		qglGetProc(glGenRenderbuffers, );
		qglGetProc(glRenderbufferStorage, );
		qglGetProc(glRenderbufferStorageMultisample, );
		qglGetProc(glGetRenderbufferParameteriv, );
		qglGetProc(glIsFramebuffer, );
		qglGetProc(glBindFramebuffer, );
		qglGetProc(glDeleteFramebuffers, );
		qglGetProc(glGenFramebuffers, );
		qglGetProc(glCheckFramebufferStatus, );
		qglGetProc(glFramebufferTexture1D, );
		qglGetProc(glFramebufferTexture2D, );
		qglGetProc(glFramebufferTexture3D, );
		qglGetProc(glFramebufferTextureLayer, );
		qglGetProc(glFramebufferRenderbuffer, );
		qglGetProc(glGetFramebufferAttachmentParameteriv, );
		qglGetProc(glBlitFramebuffer, );
		qglGetProc(glGenerateMipmap, );

		ri.Printf( PRINT_ALL, "...using GL_framebuffer_object\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_framebuffer_object" ) )
	{
		qglGetProc(glIsRenderbuffer, );
		qglGetProc(glBindRenderbuffer, );
		qglGetProc(glDeleteRenderbuffers, );
		qglGetProc(glGenRenderbuffers, );
		qglGetProc(glRenderbufferStorage, );
		qglGetProc(glRenderbufferStorageMultisample, );
		qglGetProc(glGetRenderbufferParameteriv, );
		qglGetProc(glIsFramebuffer, );
		qglGetProc(glBindFramebuffer, );
		qglGetProc(glDeleteFramebuffers, );
		qglGetProc(glGenFramebuffers, );
		qglGetProc(glCheckFramebufferStatus, );
		qglGetProc(glFramebufferTexture1D, );
		qglGetProc(glFramebufferTexture2D, );
		qglGetProc(glFramebufferTexture3D, );
		qglGetProc(glFramebufferTextureLayer, );
		qglGetProc(glFramebufferRenderbuffer, );
		qglGetProc(glGetFramebufferAttachmentParameteriv, );
		qglGetProc(glBlitFramebuffer, );
		qglGetProc(glGenerateMipmap, );

		ri.Printf( PRINT_ALL, "...using GL_ARB_framebuffer_object\n" );
	}
	else
	{
		qglIsRenderbuffer = NULL;
		qglBindRenderbuffer = NULL;
		qglDeleteRenderbuffers = NULL;
		qglGenRenderbuffers = NULL;
		qglRenderbufferStorage = NULL;
		qglRenderbufferStorageMultisample = NULL;
		qglGetRenderbufferParameteriv = NULL;
		qglIsFramebuffer = NULL;
		qglBindFramebuffer = NULL;
		qglDeleteFramebuffers = NULL;
		qglGenFramebuffers = NULL;
		qglCheckFramebufferStatus = NULL;
		qglFramebufferTexture1D = NULL;
		qglFramebufferTexture2D = NULL;
		qglFramebufferTexture3D = NULL;
		qglFramebufferTextureLayer = NULL;
		qglFramebufferRenderbuffer = NULL;
		qglGetFramebufferAttachmentParameteriv = NULL;
		qglBlitFramebuffer = NULL;
		qglGenerateMipmap = NULL;
		
		ri.Printf( PRINT_ALL, "...GL_ARB_framebuffer_object not found\n" );
	}

	// GL_ARB_occlusion_query, mandatory since OpenGL 1.5
	if ( !r_ext_occlusion_query->integer ) {
		qglGenQueriesARB = NULL;
		qglDeleteQueriesARB = NULL;
		qglIsQueryARB = NULL;
		qglBeginQueryARB = NULL;
		qglEndQueryARB = NULL;
		qglGetQueryivARB = NULL;
		qglGetQueryObjectivARB = NULL;
		qglGetQueryObjectuivARB = NULL;

		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_occlusion_query\n" );
	} else if ( GLversion >= 0x0105 ) {
		qglGetProc(glGenQueries, ARB);
		qglGetProc(glDeleteQueries, ARB);
		qglGetProc(glIsQuery, ARB);
		qglGetProc(glBeginQuery, ARB);
		qglGetProc(glEndQuery, ARB);
		qglGetProc(glGetQueryiv, ARB);
		qglGetProc(glGetQueryObjectiv, ARB);
		qglGetProc(glGetQueryObjectuiv, ARB);
		ri.Printf( PRINT_ALL, "...using GL_occlusion_query\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_occlusion_query" ) ) {
		qglGetProc(glGenQueriesARB, );
		qglGetProc(glDeleteQueriesARB, );
		qglGetProc(glIsQueryARB, );
		qglGetProc(glBeginQueryARB, );
		qglGetProc(glEndQueryARB, );
		qglGetProc(glGetQueryivARB, );
		qglGetProc(glGetQueryObjectivARB, );
		qglGetProc(glGetQueryObjectuivARB, );
		ri.Printf( PRINT_ALL, "...using GL_ARB_occlusion_query\n" );
	} else {
		qglGenQueriesARB = NULL;
		qglDeleteQueriesARB = NULL;
		qglIsQueryARB = NULL;
		qglBeginQueryARB = NULL;
		qglEndQueryARB = NULL;
		qglGetQueryivARB = NULL;
		qglGetQueryObjectivARB = NULL;
		qglGetQueryObjectuivARB = NULL;

		ri.Printf( PRINT_ALL, "...GL_ARB_occlusion_query not found\n" );
	}

	// GL_EXT_timer_query, in core since OpenGL 3.3
	if ( !r_ext_timer_query->integer || !qglGenQueriesARB ) {
		glGlobals.timerQuery = 0;
		qglGetQueryObjecti64vEXT = NULL;
		qglGetQueryObjectui64vEXT = NULL;

		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_timer_query\n" );
	} else if ( GLversion >= 0x0303 ) {
		qglGetProc(glGetQueryObjecti64v, EXT);
		qglGetProc(glGetQueryObjectui64v, EXT);
		qglGenQueriesARB( 1, &glGlobals.timerQuery );
		ri.Printf( PRINT_ALL, "...using GL_timer_query\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_EXT_timer_query" ) ) {
		qglGetProc(glGetQueryObjecti64vEXT, );
		qglGetProc(glGetQueryObjectui64vEXT, );
		qglGenQueriesARB( 1, &glGlobals.timerQuery );
		ri.Printf( PRINT_ALL, "...using GL_ARB_timer_query\n" );
	}
	else
	{
		glGlobals.timerQuery = 0;
		qglGetQueryObjecti64vEXT = NULL;
		qglGetQueryObjectui64vEXT = NULL;

		ri.Printf( PRINT_ALL, "...GL_ARB_timer_query not found\n" );
	}

	// GL_ARB_instanced_arrays, in core since OpenGL 3.3
	if ( !r_ext_instanced_arrays->integer ) {
		qglVertexAttribDivisorARB = NULL;
		qglDrawArraysInstancedARB = NULL;
		qglDrawElementsInstancedARB = NULL;

		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_instanced_arrays\n" );
	} else if ( GLversion >= 0x0303 ) {
		qglGetProc(glVertexAttribDivisor, ARB);
		qglGetProc(glDrawArraysInstanced, ARB);
		qglGetProc(glDrawElementsInstanced, ARB);

		ri.Printf( PRINT_ALL, "...using GL_instanced_arrays\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_instanced_arrays" ) ) {
		qglGetProc(glVertexAttribDivisorARB, );
		qglGetProc(glDrawArraysInstancedARB, );
		qglGetProc(glDrawElementsInstancedARB, );

		ri.Printf( PRINT_ALL, "...using GL_ARB_instanced_arrays\n" );
	}
	else
	{
		qglVertexAttribDivisorARB = NULL;
		qglDrawArraysInstancedARB = NULL;
		qglDrawElementsInstancedARB = NULL;

		ri.Printf( PRINT_ALL, "...GL_ARB_instanced_arrays not found\n" );
	}

	// GL_ARB_separate_stencil, part of 2.0 but not a separate extension
	if ( !r_ext_separate_stencil->integer ) {
		qglStencilFuncSeparate = NULL;
		qglStencilOpSeparate = NULL;
		qglStencilMaskSeparate = NULL;

		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_separate_stencil\n" );
	} else if ( GLversion >= 0x0200 ) {
		qglGetProc(glStencilFuncSeparate, );
		qglGetProc(glStencilOpSeparate, );
		qglGetProc(glStencilMaskSeparate, );

		ri.Printf( PRINT_ALL, "...using GL_separate_stencil\n" );
	} else {
		qglStencilFuncSeparate = NULL;
		qglStencilOpSeparate = NULL;
		qglStencilMaskSeparate = NULL;

		ri.Printf( PRINT_ALL, "...GL_ARB_separate_stencil not found\n" );
	}

	// GL_{AMD/ARB}_debug_output, not in core
	if ( !r_ext_debug_output->integer ) {
		qglDebugMessageControlARB = NULL;
		qglDebugMessageInsertARB = NULL;
		qglDebugMessageCallbackARB = NULL;
		qglGetDebugMessageLogARB = NULL;

		qglDebugMessageEnableAMD = NULL;
		qglDebugMessageInsertAMD = NULL;
		qglDebugMessageCallbackAMD = NULL;
		qglGetDebugMessageLogAMD = NULL;

		ri.Printf( PRINT_ALL, "...ignoring GL_ARB_debug_output\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_ARB_debug_output" ) ) {
		qglGetProc(glDebugMessageControlARB, );
		qglGetProc(glDebugMessageInsertARB, );
		qglGetProc(glDebugMessageCallbackARB, );
		qglGetProc(glGetDebugMessageLogARB, );

		qglDebugMessageEnableAMD = NULL;
		qglDebugMessageInsertAMD = NULL;
		qglDebugMessageCallbackAMD = NULL;
		qglGetDebugMessageLogAMD = NULL;

		ri.Printf( PRINT_ALL, "...using GL_ARB_debug_output\n" );
	} else if ( GLversion &&
		    GLimp_HaveExtension( "GL_AMD_debug_output" ) ) {
		qglDebugMessageControlARB = NULL;
		qglDebugMessageInsertARB = NULL;
		qglDebugMessageCallbackARB = NULL;
		qglGetDebugMessageLogARB = NULL;

		qglGetProc(glDebugMessageEnableAMD, );
		qglGetProc(glDebugMessageInsertAMD, );
		qglGetProc(glDebugMessageCallbackAMD, );
		qglGetProc(glGetDebugMessageLogAMD, );

		ri.Printf( PRINT_ALL, "...using GL_AMD_debug_output\n" );
	} else {
		qglDebugMessageControlARB = NULL;
		qglDebugMessageInsertARB = NULL;
		qglDebugMessageCallbackARB = NULL;
		qglGetDebugMessageLogARB = NULL;

		qglDebugMessageEnableAMD = NULL;
		qglDebugMessageInsertAMD = NULL;
		qglDebugMessageCallbackAMD = NULL;
		qglGetDebugMessageLogAMD = NULL;

		ri.Printf( PRINT_ALL, "...GL_ARB_debug_output not found\n" );
	}

	if( !r_ext_gpu_shader4->integer ) {
		glGlobals.gpuShader4 = qfalse;
		ri.Printf( PRINT_ALL, "...ignoring GL_EXT_gpu_shader4\n" );
	} else if( GLversion >= 0x300 ) {
		glGlobals.gpuShader4 = qtrue;
		ri.Printf( PRINT_ALL, "...using GL_gpu_shader4\n" );
	} else if( GLversion &&
		   GLimp_HaveExtension( "GL_EXT_gpu_shader4" ) ) {
		glGlobals.gpuShader4 = qtrue;
		ri.Printf( PRINT_ALL, "...using GL_EXT_gpu_shader4\n" );
	} else {
		glGlobals.gpuShader4 = qfalse;
		ri.Printf( PRINT_ALL, "...GL_EXT_gpu_shader4 not found\n" );
	}

	// GL_ARB_blend_func_extended, mandatory since OpenGL 3.3
	if( !r_ext_blend_func_extended->integer ) {
		qglBindFragDataLocationIndexed = NULL;
		qglGetFragDataIndex = NULL;
		ri.Printf( PRINT_DEVELOPER, "...ignoring GL_ARB_blend_func_extended\n" );
	} else if( GLversion >= 0x303 ) {
		qglGetProc( glBindFragDataLocationIndexed, );
		qglGetProc( glGetFragDataIndex, );
		ri.Printf( PRINT_DEVELOPER, "...ignoring GL_blend_func_extended\n" );
	} else if( GLversion &&
		   GLimp_HaveExtension( "GL_ARB_blend_func_extended" ) ) {
		qglGetProc( glBindFragDataLocationIndexed, );
		qglGetProc( glGetFragDataIndex, );
		ri.Printf( PRINT_DEVELOPER, "...ignoring GL_ARB_blend_func_extended\n" );
	} else {
		qglBindFragDataLocationIndexed = NULL;
		qglGetFragDataIndex = NULL;
		ri.Printf( PRINT_DEVELOPER, "...GL_ARB_blend_func_extended not found\n" );
	}
}

#define R_MODE_FALLBACK 3 // 640 * 480

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( void )
{
	int GLmajor, GLminor;

	r_allowSoftwareGL = ri.Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	r_sdlDriver = ri.Cvar_Get( "r_sdlDriver", "", CVAR_ROM );
	r_allowResize = ri.Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE );
	r_centerWindow = ri.Cvar_Get( "r_centerWindow", "0", CVAR_ARCHIVE );

	if( ri.Cvar_VariableIntegerValue( "com_abnormalExit" ) )
	{
		ri.Cvar_Set( "r_mode", va( "%d", R_MODE_FALLBACK ) );
		ri.Cvar_Set( "r_fullscreen", "0" );
		ri.Cvar_Set( "r_centerWindow", "0" );
		ri.Cvar_Set( "com_abnormalExit", "0" );
	}

	ri.Sys_SetEnv( "SDL_VIDEO_CENTERED", r_centerWindow->integer ? "1" : "" );

	ri.Sys_GLimpInit( );

#ifdef SDL_VIDEO_DRIVER_X11
	XInitThreads( );
#endif
	renderThreadID = SDL_ThreadID( );

	// Create the window and set up the context
	if(GLimp_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer, r_noborder->integer))
		goto success;

	// Try again, this time in a platform specific "safe mode"
	ri.Sys_GLimpSafeInit( );

	if(GLimp_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer, qfalse))
		goto success;

	// Finally, try the default screen resolution
	if( r_mode->integer != R_MODE_FALLBACK )
	{
		ri.Printf( PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n",
				r_mode->integer, R_MODE_FALLBACK );

		if(GLimp_StartDriverAndSetMode(R_MODE_FALLBACK, qfalse, qfalse))
			goto success;
	}

	// Nothing worked, give up
	ri.Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );

success:
	// This values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;
	glConfig.deviceSupportsGamma = SDL_SetGamma( 1.0f, 1.0f, 1.0f ) >= 0;

	// Mysteriously, if you use an NVidia graphics card and multiple monitors,
	// SDL_SetGamma will incorrectly return false... the first time; ask
	// again and you get the correct answer. This is a suspected driver bug, see
	// http://bugzilla.icculus.org/show_bug.cgi?id=4316
	glConfig.deviceSupportsGamma = SDL_SetGamma( 1.0f, 1.0f, 1.0f ) >= 0;

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (char *) qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (char *) qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	if (*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n')
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
	Q_strncpyz( glConfig.version_string, (char *) qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	sscanf( glConfig.version_string, "%d.%d", &GLmajor, &GLminor );
	Q_strncpyz( glConfig.extensions_string, (char *) qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	// initialize extensions
	GLimp_InitExtensions( (GLmajor << 8) | GLminor );

	ri.Cvar_Get( "r_availableModes", "", CVAR_ROM );

	// This depends on SDL_INIT_VIDEO, hence having it here
	ri.IN_Init( );
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
	// don't flip if drawing to front buffer
	if ( Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 )
	{
		SDL_GL_SwapBuffers();
	}

	if( r_fullscreen->modified )
	{
		qboolean    fullscreen;
		qboolean    needToToggle = qtrue;
		qboolean    sdlToggled = qfalse;
		SDL_Surface *s = SDL_GetVideoSurface( );

		if( s )
		{
			// Find out the current state
			fullscreen = !!( s->flags & SDL_FULLSCREEN );
				
			if( r_fullscreen->integer && ri.Cvar_VariableIntegerValue( "in_nograb" ) )
			{
				ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
				ri.Cvar_Set( "r_fullscreen", "0" );
				r_fullscreen->modified = qfalse;
			}

			// Is the state we want different from the current state?
			needToToggle = !!r_fullscreen->integer != fullscreen;

			if( needToToggle )
				sdlToggled = SDL_WM_ToggleFullScreen( s );
		}

		if( needToToggle )
		{
			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if( !sdlToggled )
				ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart");

			ri.IN_Restart( );
		}

		r_fullscreen->modified = qfalse;
	}
}



#ifdef SMP
/*
===========================================================

SMP acceleration

===========================================================
*/

/*
 * I have no idea if this will even work...most platforms don't offer
 * thread-safe OpenGL libraries, and it looks like the original Linux
 * code counted on each thread claiming the GL context with glXMakeCurrent(),
 * which you can't currently do in SDL. We'll just have to hope for the best.
 */

static SDL_mutex *smpMutex = NULL;
static SDL_cond *renderCommandsEvent = NULL;
static SDL_cond *renderCompletedEvent = NULL;
static void (*glimpRenderThread)( void ) = NULL;
static SDL_Thread *renderThread = NULL;

/*
===============
GLimp_ShutdownRenderThread
===============
*/
static void GLimp_ShutdownRenderThread(void)
{
	if (renderThread != NULL)
	{
		SDL_WaitThread(renderThread, NULL);
		renderThread = NULL;
	}

	if (smpMutex != NULL)
	{
		SDL_DestroyMutex(smpMutex);
		smpMutex = NULL;
	}

	if (renderCommandsEvent != NULL)
	{
		SDL_DestroyCond(renderCommandsEvent);
		renderCommandsEvent = NULL;
	}

	if (renderCompletedEvent != NULL)
	{
		SDL_DestroyCond(renderCompletedEvent);
		renderCompletedEvent = NULL;
	}

	glimpRenderThread = NULL;
	renderThreadID = SDL_ThreadID( );
}

/*
===============
GLimp_RenderThreadWrapper
===============
*/
static int GLimp_RenderThreadWrapper( void *arg )
{
	// These printfs cause race conditions which mess up the console output
	//ri.Printf( PRINT_ALL, "Render thread starting\n" );
	GLimp_SetCurrentContext( &backend_context );
	renderThreadID = SDL_ThreadID( );

	glimpRenderThread();

	GLimp_SetCurrentContext( NULL );

	ri.Printf( PRINT_ALL, "Render thread terminating\n" );

	return 0;
}

/*
===============
GLimp_SpawnRenderThread
===============
*/
qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
	static qboolean warned = qfalse;
	if (!warned)
	{
		ri.Printf( PRINT_WARNING, "You enable r_smp at your own risk!\n");
		warned = qtrue;
	}

#if !defined(MACOS_X) && !defined(WIN32) && !defined (SDL_VIDEO_DRIVER_X11)
	return qfalse;  /* better safe than sorry for now. */
#endif

	if (renderThread != NULL)  /* hopefully just a zombie at this point... */
	{
		ri.Printf( PRINT_ALL, "Already a render thread? Trying to clean it up...\n" );
		GLimp_ShutdownRenderThread();
	}

	smpMutex = SDL_CreateMutex();
	if (smpMutex == NULL)
	{
		ri.Printf( PRINT_ERROR, "smpMutex creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderCommandsEvent = SDL_CreateCond();
	if (renderCommandsEvent == NULL)
	{
		ri.Printf( PRINT_ERROR, "renderCommandsEvent creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderCompletedEvent = SDL_CreateCond();
	if (renderCompletedEvent == NULL)
	{
		ri.Printf( PRINT_ERROR, "renderCompletedEvent creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	glimpRenderThread = function;
	renderThread = SDL_CreateThread(GLimp_RenderThreadWrapper, NULL);
	if ( renderThread == NULL )
	{
		ri.Printf( PRINT_ERROR, "SDL_CreateThread() returned %s", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}
	else
	{
		// tma 01/09/07: don't think this is necessary anyway?
		//
		// !!! FIXME: No detach API available in SDL!
		//ret = pthread_detach( renderThread );
		//if ( ret ) {
		//ri.Printf( PRINT_ALL, "pthread_detach returned %d: %s", ret, strerror( ret ) );
		//}
	}

	return qtrue;
}

static volatile void    *smpData = NULL;
static volatile qboolean smpDataReady;

/*
===============
GLimp_RendererSleep
===============
*/
void *GLimp_RendererSleep( void )
{
	void  *data = NULL;

	SDL_LockMutex(smpMutex);
	{
		smpData = NULL;
		smpDataReady = qfalse;

		// after this, the front end can exit GLimp_FrontEndSleep
		do {
			SDL_CondSignal(renderCompletedEvent);
			SDL_CondWait(renderCommandsEvent, smpMutex);
		} while( !smpDataReady);

		data = (void *)smpData;
	}
	SDL_UnlockMutex(smpMutex);

	return data;
}

/*
===============
GLimp_FrontEndSleep
===============
*/
void GLimp_FrontEndSleep( void )
{
	SDL_LockMutex(smpMutex);
	{
		while ( smpData )
			SDL_CondWait(renderCompletedEvent, smpMutex);
	}
	SDL_UnlockMutex(smpMutex);
}

/*
===============
GLimp_WakeRenderer
===============
*/
void GLimp_WakeRenderer( void *data )
{
	SDL_LockMutex(smpMutex);
	{
		assert( smpData == NULL );
		smpData = data;
		smpDataReady = qtrue;

		// after this, the renderer can continue through GLimp_RendererSleep
		SDL_CondSignal(renderCommandsEvent);
	}
	SDL_UnlockMutex(smpMutex);
}

/*
===============
GLimp_InBackend
===============
*/
qboolean GLimp_InBackend( void )
{
	return SDL_ThreadID() == renderThreadID;
}
#else

// No SMP - stubs
void GLimp_RenderThreadWrapper( void *arg )
{
}

qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
	ri.Printf( PRINT_WARNING, "ERROR: SMP support was disabled at compile time\n");
	return qfalse;
}

void *GLimp_RendererSleep( void )
{
	return NULL;
}

void GLimp_FrontEndSleep( void )
{
}

void GLimp_WakeRenderer( void *data )
{
}

qboolean GLimp_InBackend( void )
{
	return qtrue;
}
#endif
