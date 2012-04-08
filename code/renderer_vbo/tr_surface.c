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
// tr_surf.c
#include TR_CONFIG_H
#include TR_LOCAL_H

#if idppc_altivec && !defined(MACOS_X)
#include <altivec.h>
#endif

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/


//============================================================================


/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color,
			 float s1, float t1, float s2, float t2 ) {
	int			ndx;

	ndx = tess.numVertexes;

	tess.vertexPtr[ndx+0].xyz[0] = origin[0] + left[0] + up[0];
	tess.vertexPtr[ndx+0].xyz[1] = origin[1] + left[1] + up[1];
	tess.vertexPtr[ndx+0].xyz[2] = origin[2] + left[2] + up[2];
	tess.vertexPtr[ndx+0].fogNum = (float)tess.fogNum;

	tess.vertexPtr[ndx+1].xyz[0] = origin[0] - left[0] + up[0];
	tess.vertexPtr[ndx+1].xyz[1] = origin[1] - left[1] + up[1];
	tess.vertexPtr[ndx+1].xyz[2] = origin[2] - left[2] + up[2];
	tess.vertexPtr[ndx+1].fogNum = (float)tess.fogNum;

	tess.vertexPtr[ndx+2].xyz[0] = origin[0] - left[0] - up[0];
	tess.vertexPtr[ndx+2].xyz[1] = origin[1] - left[1] - up[1];
	tess.vertexPtr[ndx+2].xyz[2] = origin[2] - left[2] - up[2];
	tess.vertexPtr[ndx+2].fogNum = (float)tess.fogNum;

	tess.vertexPtr[ndx+3].xyz[0] = origin[0] + left[0] - up[0];
	tess.vertexPtr[ndx+3].xyz[1] = origin[1] + left[1] - up[1];
	tess.vertexPtr[ndx+3].xyz[2] = origin[2] + left[2] - up[2];
	tess.vertexPtr[ndx+3].fogNum = (float)tess.fogNum;

	// sprites don't use normal, so I reuse it to store the
	// shadertimes
	tess.vertexPtr[ndx+0].normal[0] = tess.shaderTime;
	tess.vertexPtr[ndx+0].normal[1] = 0.0f;
	tess.vertexPtr[ndx+0].normal[2] = 0.0f;
	tess.vertexPtr[ndx+1].normal[0] = tess.shaderTime;
	tess.vertexPtr[ndx+1].normal[1] = 0.0f;
	tess.vertexPtr[ndx+1].normal[2] = 0.0f;
	tess.vertexPtr[ndx+2].normal[0] = tess.shaderTime;
	tess.vertexPtr[ndx+2].normal[1] = 0.0f;
	tess.vertexPtr[ndx+2].normal[2] = 0.0f;
	tess.vertexPtr[ndx+3].normal[0] = tess.shaderTime;
	tess.vertexPtr[ndx+3].normal[1] = 0.0f;
	tess.vertexPtr[ndx+3].normal[2] = 0.0f;

	// standard square texture coordinates
	tess.vertexPtr[ndx].tc1[0] = tess.vertexPtr[ndx].tc2[0] = s1;
	tess.vertexPtr[ndx].tc1[1] = tess.vertexPtr[ndx].tc2[1] = t1;

	tess.vertexPtr[ndx+1].tc1[0] = tess.vertexPtr[ndx+1].tc2[0] = s2;
	tess.vertexPtr[ndx+1].tc1[1] = tess.vertexPtr[ndx+1].tc2[1] = t1;

	tess.vertexPtr[ndx+2].tc1[0] = tess.vertexPtr[ndx+2].tc2[0] = s2;
	tess.vertexPtr[ndx+2].tc1[1] = tess.vertexPtr[ndx+2].tc2[1] = t2;

	tess.vertexPtr[ndx+3].tc1[0] = tess.vertexPtr[ndx+3].tc2[0] = s1;
	tess.vertexPtr[ndx+3].tc1[1] = tess.vertexPtr[ndx+3].tc2[1] = t2;

	// constant color all the way around
	// should this be identity and let the shader specify from entity?
	tess.vertexPtr[ndx].color[0] = color[0];
	tess.vertexPtr[ndx].color[1] = color[1];
	tess.vertexPtr[ndx].color[2] = color[2];
	tess.vertexPtr[ndx].color[3] = color[3];
	tess.vertexPtr[ndx+1].color[0] = color[0];
	tess.vertexPtr[ndx+1].color[1] = color[1];
	tess.vertexPtr[ndx+1].color[2] = color[2];
	tess.vertexPtr[ndx+1].color[3] = color[3];
	tess.vertexPtr[ndx+2].color[0] = color[0];
	tess.vertexPtr[ndx+2].color[1] = color[1];
	tess.vertexPtr[ndx+2].color[2] = color[2];
	tess.vertexPtr[ndx+2].color[3] = color[3];
	tess.vertexPtr[ndx+3].color[0] = color[0];
	tess.vertexPtr[ndx+3].color[1] = color[1];
	tess.vertexPtr[ndx+3].color[2] = color[2];
	tess.vertexPtr[ndx+3].color[3] = color[3];

	if ( tess.minIndex[tess.indexRange] > ndx )
		tess.minIndex[tess.indexRange] = ndx;
	if ( tess.maxIndex[tess.indexRange] < ndx + 3 )
		tess.maxIndex[tess.indexRange] = ndx + 3;

	// triangle indexes for a simple quad
	tess.indexPtr[ tess.numIndexes[tess.indexRange] ] = ndx;
	tess.indexPtr[ tess.numIndexes[tess.indexRange] + 1 ] = ndx + 1;
	tess.indexPtr[ tess.numIndexes[tess.indexRange] + 2 ] = ndx + 3;

	tess.indexPtr[ tess.numIndexes[tess.indexRange] + 3 ] = ndx + 3;
	tess.indexPtr[ tess.numIndexes[tess.indexRange] + 4 ] = ndx + 1;
	tess.indexPtr[ tess.numIndexes[tess.indexRange] + 5 ] = ndx + 2;
	
	tess.numVertexes += 4;
	tess.numIndexes[tess.indexRange] += 6;
}

/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}

/*
==============
RB_SurfaceSprite
==============
*/
static void RB_SurfaceSprite( tessMode_t mode ) {
	vec3_t		left, up;
	float		radius;

	if( mode == TESS_COUNT ) {
		tess.numVertexes += 4;
		tess.numIndexes[tess.indexRange] += 6;
		return;
	}

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	if ( backEnd.currentEntity->e.rotation == 0 ) {
		VectorScale( backEnd.viewParms.or.axis[1], radius, left );
		VectorScale( backEnd.viewParms.or.axis[2], radius, up );
	} else {
		float	s, c;
		float	ang;
		
		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin( ang );
		c = cos( ang );

		VectorScale( backEnd.viewParms.or.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.or.axis[2], left );

		VectorScale( backEnd.viewParms.or.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.or.axis[1], up );
	}
	if ( backEnd.viewParms.isMirror ) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up,
			 backEnd.currentEntity->e.shaderRGBA );
}


/*
=============
RB_SurfacePolychain
=============
*/
static void RB_SurfacePolychain( tessMode_t mode, surfaceType_t *surface ) {
	srfPoly_t *p = (srfPoly_t *)surface;
	int		i;
	int		numv;
	glVertex_t	*vertexPtr;

	if ( mode & TESS_VERTEX ) {
		vertexPtr = tess.vertexPtr + tess.numVertexes;
		
		// fan triangles into the tess array
		numv = tess.numVertexes;
		for ( i = 0; i < p->numVerts; i++ ) {
			VectorCopy ( p->verts[i].xyz, *(vec3_t *)vertexPtr->xyz );
			vertexPtr->tc1[0] = p->verts[i].st[0];
			vertexPtr->tc1[1] = p->verts[i].st[1];
			*(int *)(&vertexPtr->color) = *(int *)p->verts[ i ].modulate;
			vertexPtr->fogNum = tess.fogNum;
			
			vertexPtr++;
		}
	} else {
		numv = 0; //ERROR, should never have coordinates in VBO
	}

	if ( mode & TESS_INDEX ) {
		glIndex_t	*indexPtr = tess.indexPtr + tess.numIndexes[tess.indexRange];

		if ( tess.minIndex[tess.indexRange] > numv )
			tess.minIndex[tess.indexRange] = numv;
		if ( tess.maxIndex[tess.indexRange] < numv + p->numVerts - 1 )
			tess.maxIndex[tess.indexRange] = numv + p->numVerts - 1;
		
		// generate fan indexes into the tess array
		for ( i = 0; i < p->numVerts-2; i++ ) {
			indexPtr[0] = numv;
			indexPtr[1] = numv + i + 1;
			indexPtr[2] = numv + i + 2;
			indexPtr += 3;
		}
	}
	
	tess.numVertexes += p->numVerts;
	tess.numIndexes[tess.indexRange] += 3*(p->numVerts - 2);
}


/*
=============
RB_SurfaceTriangles
=============
*/
static void RB_SurfaceTriangles( tessMode_t mode, surfaceType_t *surface ) {
	srfTriangles_t *srf = (srfTriangles_t *)surface;
	int		i;
	drawVert_t	*dv;
	int		dlightBits;
	glIndex_t	*indexPtr;
	glVertex_t	*vertexPtr;
	int             numv;

	dlightBits = srf->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	if ( mode & TESS_VERTEX ) {
		numv = tess.numVertexes;
		
		vertexPtr = tess.vertexPtr + numv;

		dv = srf->verts;
		
		for ( i = 0 ; i < srf->numVerts ; i++, dv++ ) {
			VectorCopy( dv->xyz, *(vec3_t *)vertexPtr->xyz );
			VectorCopy( dv->normal, vertexPtr->normal );
			vertexPtr->tc1[0] = dv->st[0];
			vertexPtr->tc1[1] = dv->st[1];
			vertexPtr->tc2[0] = dv->lightmap[0];
			vertexPtr->tc2[1] = dv->lightmap[1];
			*(int *)&vertexPtr->color = *(int *)dv->color;
			vertexPtr->fogNum = tess.fogNum;
			vertexPtr++;
		}
	} else {
		numv = srf->vboStart;
	}

	if ( mode & TESS_INDEX ) {
		if ( tess.minIndex[tess.indexRange] > numv )
			tess.minIndex[tess.indexRange] = numv;
		if ( tess.maxIndex[tess.indexRange] < numv + srf->numVerts - 1 )
			tess.maxIndex[tess.indexRange] = numv + srf->numVerts - 1;
		
		indexPtr = tess.indexPtr + tess.numIndexes[tess.indexRange];
		for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
			indexPtr[ i + 0 ] = numv + srf->indexes[ i + 0 ];
			indexPtr[ i + 1 ] = numv + srf->indexes[ i + 1 ];
			indexPtr[ i + 2 ] = numv + srf->indexes[ i + 2 ];
		}
	}

	tess.numIndexes[tess.indexRange] += srf->numIndexes;
	tess.numVertexes += srf->numVerts;
}



/*
==============
RB_SurfaceBeam
==============
*/
static void RB_SurfaceBeam( tessMode_t mode ) 
{
#define NUM_BEAM_SEGS 6
	refEntity_t *e;
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	points[2 * (NUM_BEAM_SEGS+1)];
	vec3_t	*start_point, *end_point;
	vec3_t oldorigin, origin;
	glRenderState_t state;

	if( mode == TESS_COUNT ) {
		return;
	}

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );

	VectorScale( perpvec, 4, perpvec );

	start_point = &points[0];
	end_point = &points[1];
	for ( i = 0; i < NUM_BEAM_SEGS ; i++ )
	{
		RotatePointAroundVector( *start_point, normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
//		VectorAdd( *start_point, origin, *start_point );
		VectorAdd( *start_point, direction, *end_point );

		start_point += 2; end_point += 2;
	}
	VectorCopy( points[0], *start_point );
	VectorCopy( points[1], *end_point );

	InitState( &state );

	state.numImages = 1;
	state.image[0] = tr.whiteImage;

	state.stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;

	SetAttrVec4f( &state, AL_COLOR, 1.0f, 1.0f, 1.0f, 1.0f );

	SetAttrPointer( &state, AL_VERTEX, 0,
			3, GL_FLOAT, sizeof(vec3_t),
			points );
	GL_DrawArrays( &state, GL_TRIANGLE_STRIP, 0, 2*(NUM_BEAM_SEGS+1) );
}

//================================================================================

static void DoRailCore( tessMode_t mode,
			const vec3_t start, const vec3_t end,
			const vec3_t up, float len, float spanWidth )
{
	float		spanWidth2;
	glIndex_t	vbase;
	float		t = len / 256.0f;

	spanWidth2 = -spanWidth;

	if ( mode & TESS_VERTEX ) {
		vbase = tess.numVertexes;
		
		if ( tess.minIndex[tess.indexRange] > vbase )
			tess.minIndex[tess.indexRange] = vbase;
		if ( tess.maxIndex[tess.indexRange] < vbase + 3 )
			tess.maxIndex[tess.indexRange] = vbase + 3;

		// FIXME: use quad stamp?
		VectorMA( start, spanWidth, up, tess.vertexPtr[vbase].xyz );
		tess.vertexPtr[vbase].tc1[0] = 0;
		tess.vertexPtr[vbase].tc1[1] = 0;
		tess.vertexPtr[vbase].color[0] = backEnd.currentEntity->e.shaderRGBA[0] * 0.25;
		tess.vertexPtr[vbase].color[1] = backEnd.currentEntity->e.shaderRGBA[1] * 0.25;
		tess.vertexPtr[vbase].color[2] = backEnd.currentEntity->e.shaderRGBA[2] * 0.25;

		VectorMA( start, spanWidth2, up, tess.vertexPtr[vbase+1].xyz );
		tess.vertexPtr[vbase+1].tc1[0] = 0;
		tess.vertexPtr[vbase+1].tc1[1] = 1;
		tess.vertexPtr[vbase+1].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr[vbase+1].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr[vbase+1].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
		
		VectorMA( end, spanWidth, up, tess.vertexPtr[vbase+2].xyz );
		tess.vertexPtr[vbase+2].tc1[0] = t;
		tess.vertexPtr[vbase+2].tc1[1] = 0;
		tess.vertexPtr[vbase+2].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr[vbase+2].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr[vbase+2].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
		
		VectorMA( end, spanWidth2, up, tess.vertexPtr[vbase+3].xyz );
		tess.vertexPtr[vbase+3].tc1[0] = t;
		tess.vertexPtr[vbase+3].tc1[1] = 1;
		tess.vertexPtr[vbase+3].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr[vbase+3].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr[vbase+3].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
	} else {
		vbase = 0;
	}
	
	if ( mode & TESS_INDEX ) {
		int	idx = tess.numIndexes[tess.indexRange];
		tess.indexPtr[idx] = vbase;
		tess.indexPtr[idx+1] = vbase + 1;
		tess.indexPtr[idx+2] = vbase + 2;
		
		tess.indexPtr[idx+3] = vbase + 2;
		tess.indexPtr[idx+4] = vbase + 1;
		tess.indexPtr[idx+5] = vbase + 3;
	}

	tess.numVertexes += 4;
	tess.numIndexes[tess.indexRange] += 6;
}

static void DoRailDiscs( tessMode_t mode, int numSegs,
			 const vec3_t start, const vec3_t dir,
			 const vec3_t right, const vec3_t up )
{
	int i;
	vec3_t	pos[4];
	vec3_t	v;
	int		spanWidth = r_railWidth->integer;
	glIndex_t	vbase;
	float c, s;
	float		scale = 0.25;

	if ( numSegs > 1 )
		numSegs--;
	if ( !numSegs )
		return;

	for ( i = 0; i < 4; i++ )
	{
		c = cos( DEG2RAD( 45 + i * 90 ) );
		s = sin( DEG2RAD( 45 + i * 90 ) );
		v[0] = ( right[0] * c + up[0] * s ) * scale * spanWidth;
		v[1] = ( right[1] * c + up[1] * s ) * scale * spanWidth;
		v[2] = ( right[2] * c + up[2] * s ) * scale * spanWidth;
		VectorAdd( start, v, pos[i] );

		if ( numSegs > 1 )
		{
			// offset by 1 segment if we're doing a long distance shot
			VectorAdd( pos[i], dir, pos[i] );
		}
	}

	if ( mode & TESS_VERTEX ) {
		vbase = tess.numVertexes;
		for ( i = 0; i < numSegs; i++ )
		{
			int j;
			
			for ( j = 0; j < 4; j++ )
			{
				VectorCopy( pos[j], tess.vertexPtr[vbase].xyz );
				tess.vertexPtr[vbase].tc1[0] = ( j < 2 );
				tess.vertexPtr[vbase].tc1[1] = ( j && j != 3 );
				tess.vertexPtr[vbase].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
				tess.vertexPtr[vbase].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
				tess.vertexPtr[vbase].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
				vbase++;
				
				VectorAdd( pos[j], dir, pos[j] );
			}
		}
		vbase = tess.numVertexes;
	} else {
		vbase = 0;
	}

	if ( mode & TESS_INDEX ) {
		for ( i = 0; i < numSegs; i++ )
		{
			int iwrite = tess.numIndexes[tess.indexRange];
			tess.indexPtr[iwrite++] = vbase + 0;
			tess.indexPtr[iwrite++] = vbase + 1;
			tess.indexPtr[iwrite++] = vbase + 3;
			tess.indexPtr[iwrite++] = vbase + 3;
			tess.indexPtr[iwrite++] = vbase + 1;
			tess.indexPtr[iwrite++] = vbase + 2;
			vbase += 4;
		}
	}

	tess.numVertexes += numSegs * 4;
	tess.numIndexes[tess.indexRange] += numSegs * 6;
}

/*
** RB_SurfaceRailRinges
*/
static void RB_SurfaceRailRings( tessMode_t mode ) {
	refEntity_t *e;
	int			numSegs;
	int			len;
	vec3_t		vec;
	vec3_t		right, up;
	vec3_t		start, end;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	MakeNormalVectors( vec, right, up );
	numSegs = ( len ) / r_railSegmentLength->value;
	if ( numSegs <= 0 ) {
		numSegs = 1;
	}

	VectorScale( vec, r_railSegmentLength->value, vec );

	DoRailDiscs( mode, numSegs, start, vec, right, up );
}

/*
** RB_SurfaceRailCore
*/
static void RB_SurfaceRailCore( tessMode_t mode ) {
	refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	DoRailCore( mode, start, end, right, len, r_railCoreWidth->integer );
}

/*
** RB_SurfaceLightningBolt
*/
static void RB_SurfaceLightningBolt( tessMode_t mode ) {
	refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;
	int			i;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	for ( i = 0 ; i < 4 ; i++ ) {
		vec3_t	temp;

		DoRailCore( mode, start, end, right, len, 8 );
		RotatePointAroundVector( temp, vec, right, 45 );
		VectorCopy( temp, right );
	}
}



/*
** LerpMeshVertexes
*/
#if idppc_altivec
static void LerpMeshVertexes_altivec(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	glVertex_t	*vertexPtr;
	float	oldXyzScale ALIGNED(16);
	float   newXyzScale ALIGNED(16);
	float	oldNormalScale ALIGNED(16);
	float newNormalScale ALIGNED(16);
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	vertexPtr = tess.vertexPtr + tess.numVertexes;

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		vector signed short newNormalsVec0;
		vector signed short newNormalsVec1;
		vector signed int newNormalsIntVec;
		vector float newNormalsFloatVec;
		vector float newXyzScaleVec;
		vector unsigned char newNormalsLoadPermute;
		vector unsigned char newNormalsStorePermute;
		vector float zero;
		
		newNormalsStorePermute = vec_lvsl(0,(float *)&newXyzScaleVec);
		newXyzScaleVec = *(vector float *)&newXyzScale;
		newXyzScaleVec = vec_perm(newXyzScaleVec,newXyzScaleVec,newNormalsStorePermute);
		newXyzScaleVec = vec_splat(newXyzScaleVec,0);		
		newNormalsLoadPermute = vec_lvsl(0,newXyz);
		newNormalsStorePermute = vec_lvsr(0,outXyz);
		zero = (vector float)vec_splat_s8(0);
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
		       newXyz += 4, newNormals += 4, vertexPtr++ )
		{
			newNormalsLoadPermute = vec_lvsl(0,newXyz);
			newNormalsStorePermute = vec_lvsr(0,outXyz);
			newNormalsVec0 = vec_ld(0,newXyz);
			newNormalsVec1 = vec_ld(16,newXyz);
			newNormalsVec0 = vec_perm(newNormalsVec0,newNormalsVec1,newNormalsLoadPermute);
			newNormalsIntVec = vec_unpackh(newNormalsVec0);
			newNormalsFloatVec = vec_ctf(newNormalsIntVec,0);
			newNormalsFloatVec = vec_madd(newNormalsFloatVec,newXyzScaleVec,zero);
			newNormalsFloatVec = vec_perm(newNormalsFloatVec,newNormalsFloatVec,newNormalsStorePermute);
			//outXyz[0] = newXyz[0] * newXyzScale;
			//outXyz[1] = newXyz[1] * newXyzScale;
			//outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			vertexPtr->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			vertexPtr->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			vertexPtr->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vec_ste(newNormalsFloatVec,0,vertexPtr->xyz);
			vec_ste(newNormalsFloatVec,4,vertexPtr->xyz);
			vec_ste(newNormalsFloatVec,8,vertexPtr->xyz);
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
		       oldXyz += 4, newXyz += 4, oldNormals += 4,
		       newNormals += 4, vertexPtr++ )
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			vertexPtr->xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			vertexPtr->xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			vertexPtr->xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vertexPtr->normal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			vertexPtr->normal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			vertexPtr->normal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			VectorNormalize (vertexPtr->normal);
		}
   	}
}
#endif

static void LerpMeshVertexes_scalar(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	glVertex_t	*vertexPtr;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	vertexPtr = tess.vertexPtr + tess.numVertexes;

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			     newXyz += 4, newNormals += 4, vertexPtr++ )
		{

			vertexPtr->xyz[0] = newXyz[0] * newXyzScale;
			vertexPtr->xyz[1] = newXyz[1] * newXyzScale;
			vertexPtr->xyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			vertexPtr->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			vertexPtr->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			vertexPtr->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
		       oldXyz += 4, newXyz += 4, oldNormals += 4,
		       newNormals += 4, vertexPtr++ )
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			vertexPtr->xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			vertexPtr->xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			vertexPtr->xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vertexPtr->normal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			vertexPtr->normal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			vertexPtr->normal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			VectorNormalize (vertexPtr->normal);
		}
   	}
}


/*
=============
RB_SurfaceMesh
=============
*/
#if idppc_altivec
static void RB_SurfaceMesh_altivec( tessMode_t mode, md3Surface_t *surface ) {
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Doug;
	int				numVerts;
	glVertex_t		*vertexPtr;

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	Doug = tess.numVertexes;

	if ( mode & TESS_VERTEX ) {
		LerpMeshVertexes_altivec (surface, backlerp);
		
		texCoords = (float *) ((byte *)surface + surface->ofsSt);
		vertexPtr = tess.vertexPtr + Doug;

		numVerts = surface->numVerts;
		for ( j = 0; j < numVerts; j++, vertexPtr++ ) {
			vertexPtr->tc1[0] = vertexPtr->tc2[0] = texCoords[j*2+0];
			vertexPtr->tc1[1] = vertexPtr->tc2[1] = texCoords[j*2+1];
			vertexPtr->fogNum = tess.fogNum;
			vertexPtr->color[0] = 255;
			vertexPtr->color[1] = 255;
			vertexPtr->color[2] = 255;
			vertexPtr->color[3] = 255;
		}
	}

	if ( mode & TESS_INDEX ) {
		glIndex_t	*indexPtr = tess.indexPtr + tess.numIndexes[tess.indexRange];

		if ( tess.minIndex[tess.indexRange] > Doug )
			tess.minIndex[tess.indexRange] = Doug;
		if ( tess.maxIndex[tess.indexRange] < Doug + surface->numVerts - 1 )
			tess.maxIndex[tess.indexRange] = Doug + surface->numVerts - 1;

		triangles = (int *) ((byte *)surface + surface->ofsTriangles);
		indexes = surface->numTriangles * 3;

		for (j = 0 ; j < indexes ; j++) {
			*indexPtr++ = Doug + triangles[j];
		}
	}
	tess.numVertexes += surface->numVerts;
	tess.numIndexes[tess.indexRange] += 3*surface->numTriangles;
}
#endif

static void RB_SurfaceMesh_scalar( tessMode_t mode, md3Surface_t *surface ) {
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Doug;
	int				numVerts;
	glVertex_t		*vertexPtr;

	Doug = tess.numVertexes;

	if ( mode & TESS_VERTEX ) {
		if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
			backlerp = 0;
		} else  {
			backlerp = backEnd.currentEntity->e.backlerp;
		}

		LerpMeshVertexes_scalar (surface, backlerp);

		texCoords = (float *) ((byte *)surface + surface->ofsSt);
		vertexPtr = tess.vertexPtr + Doug;
		
		numVerts = surface->numVerts;
		for ( j = 0; j < numVerts; j++ ) {
			vertexPtr->tc1[0] = vertexPtr->tc2[0] = texCoords[j*2+0];
			vertexPtr->tc1[1] = vertexPtr->tc2[1] = texCoords[j*2+1];
			vertexPtr->fogNum = tess.fogNum;
			vertexPtr->color[0] = 255;
			vertexPtr->color[1] = 255;
			vertexPtr->color[2] = 255;
			vertexPtr->color[3] = 255;
			vertexPtr++;
		}
	}

	if ( mode & TESS_INDEX ) {
		glIndex_t	*indexPtr = tess.indexPtr + tess.numIndexes[tess.indexRange];

		if ( tess.minIndex[tess.indexRange] > Doug )
			tess.minIndex[tess.indexRange] = Doug;
		if ( tess.maxIndex[tess.indexRange] < Doug + surface->numVerts - 1 )
			tess.maxIndex[tess.indexRange] = Doug + surface->numVerts - 1;
		
		triangles = (int *) ((byte *)surface + surface->ofsTriangles);
		indexes = surface->numTriangles * 3;
		
		for (j = 0 ; j < indexes ; j++) {
			*indexPtr++ = Doug + triangles[j];
		}
	}
	tess.numVertexes += surface->numVerts;
	tess.numIndexes[tess.indexRange] += 3*surface->numTriangles;
}

void RB_SurfaceMesh( tessMode_t mode, surfaceType_t *surf) {
	md3Surface_t *surface = (md3Surface_t *)surf;
#if idppc_altivec
	if (com_altivec->integer) {
		RB_SurfaceMesh_altivec( mode, surface );
		return;
	}
#endif // idppc_altivec
	RB_SurfaceMesh_scalar( mode, surface );
}


/*
==============
RB_SurfaceFace
==============
*/
#if id386_sse >= 2
void RB_SurfaceFace_sse2( tessMode_t mode, srfSurfaceFace_t *surf );
#endif
static ID_INLINE void RB_SurfaceFace_scalar( tessMode_t mode,
					     srfSurfaceFace_t *surf ) {
	int			i;
	unsigned	*indices;
	float		*v;
	int			Bob;
	int			numPoints;
	int			dlightBits;
	glVertex_t	*vertexPtr;

	dlightBits = surf->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	if ( mode & TESS_VERTEX ) {
		Bob = tess.numVertexes;

		v = surf->points[0];
		
		numPoints = surf->numPoints;
		
		vertexPtr = tess.vertexPtr + tess.numVertexes;
		
		for ( i = 0, v = surf->points[0]; i < numPoints;
		      i++, v += VERTEXSIZE, vertexPtr++ ) {
			VectorCopy( surf->plane.normal, vertexPtr->normal );
			
			VectorCopy ( v, vertexPtr->xyz);
			vertexPtr->fogNum = tess.fogNum;
			vertexPtr->tc1[0] = v[3];
			vertexPtr->tc1[1] = v[4];
			vertexPtr->tc2[0] = v[5];
			vertexPtr->tc2[1] = v[6];
			*(int *)&vertexPtr->color = *(int *)&v[7];
		}
	} else {
		Bob = surf->vboStart;
	}

	if ( mode & TESS_INDEX ) {
		glIndex_t *indexPtr = tess.indexPtr + tess.numIndexes[tess.indexRange];
		
		if ( tess.minIndex[tess.indexRange] > Bob )
			tess.minIndex[tess.indexRange] = Bob;
		if ( tess.maxIndex[tess.indexRange] < Bob + surf->numPoints - 1 )
			tess.maxIndex[tess.indexRange] = Bob + surf->numPoints - 1;
		
		indices = ( unsigned * ) ( ( ( char  * ) surf ) + surf->ofsIndices );
		for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
			indexPtr[i] = indices[i] + Bob;
		}
	}

	tess.numVertexes += surf->numPoints;
	tess.numIndexes[tess.indexRange] += surf->numIndices;
}

static void RB_SurfaceFace( tessMode_t mode, surfaceType_t *surface ) {
	srfSurfaceFace_t *surf = (srfSurfaceFace_t *)surface;
	RB_SurfaceFace_scalar ( mode, surf );
}


static float	LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t		world;
	float		d;

	// never let it go negative
	if ( r_lodCurveError->value < 0 ) {
		return 0;
	}

	world[0] = local[0] * backEnd.or.axis[0][0] + local[1] * backEnd.or.axis[1][0] + 
		local[2] * backEnd.or.axis[2][0] + backEnd.or.origin[0];
	world[1] = local[0] * backEnd.or.axis[0][1] + local[1] * backEnd.or.axis[1][1] + 
		local[2] * backEnd.or.axis[2][1] + backEnd.or.origin[1];
	world[2] = local[0] * backEnd.or.axis[0][2] + local[1] * backEnd.or.axis[1][2] + 
		local[2] * backEnd.or.axis[2][2] + backEnd.or.origin[2];

	VectorSubtract( world, backEnd.viewParms.or.origin, world );
	d = DotProduct( world, backEnd.viewParms.or.axis[0] );

	if ( d < 0 ) {
		d = -d;
	}
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	}

	return r_lodCurveError->value / d;
}

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
static void RB_SurfaceGrid( tessMode_t mode, surfaceType_t *surface ) {
	srfGridMesh_t *cv = (srfGridMesh_t *)surface;
	int		i, j;
	glVertex_t	*vertexPtr;
	drawVert_t	*dv;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		baseVertex;
	int		dlightBits;

	dlightBits = cv->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	// determine the allowable discrepance
	if ( r_ext_vertex_buffer_object->integer ) {
		// always render max res for VBOs
		lodError = r_lodCurveError->value;
	} else {
		lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );
	}

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;

	if ( mode & TESS_VERTEX ) {
		baseVertex = tess.numVertexes;
		vertexPtr = tess.vertexPtr + baseVertex;
		
		for ( i = 0 ; i < lodHeight ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++, vertexPtr++ ) {
				dv = cv->verts + heightTable[ i ] * cv->width
					+ widthTable[ j ];
				
				VectorCopy( dv->xyz, vertexPtr->xyz );
				vertexPtr->tc1[0] = dv->st[0];
				vertexPtr->tc1[1] = dv->st[1];
				vertexPtr->tc2[0] = dv->lightmap[0];
				vertexPtr->tc2[1] = dv->lightmap[1];
				VectorCopy( dv->normal, vertexPtr->normal );
				*(int *)&vertexPtr->color = *(int *)dv->color;
				vertexPtr->fogNum = tess.fogNum;
			}
		}
	} else {
		baseVertex = cv->vboStart;
	}
	
	if ( mode & TESS_INDEX ) {
		int		w, h;
		glIndex_t	*indexPtr = tess.indexPtr + tess.numIndexes[tess.indexRange];

		if ( tess.minIndex[tess.indexRange] > baseVertex )
			tess.minIndex[tess.indexRange] = baseVertex;
		if ( tess.maxIndex[tess.indexRange] < baseVertex + lodWidth*lodHeight - 1 )
			tess.maxIndex[tess.indexRange] = baseVertex + lodWidth*lodHeight - 1;

		// add the indexes
		
		h = lodHeight - 1;
		w = lodWidth - 1;
		
		for (i = 0 ; i < h ; i++) {
			for (j = 0 ; j < w ; j++) {
				glIndex_t	v1, v2, v3, v4;
				
				// vertex order to be reckognized as tristrips
				v1 = baseVertex + i*lodWidth + j + 1;
				v2 = v1 - 1;
				v3 = v2 + lodWidth;
				v4 = v3 + 1;
					
				*indexPtr++ = v2;
				*indexPtr++ = v3;
				*indexPtr++ = v1;
					
				*indexPtr++ = v1;
				*indexPtr++ = v3;
				*indexPtr++ = v4;
			}
		}
	}
	
	tess.numVertexes += lodWidth * lodHeight;
	tess.numIndexes[tess.indexRange] += 6 * (lodWidth - 1) * (lodHeight - 1);
}


/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
RB_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
static void RB_SurfaceAxis( tessMode_t mode ) {
	static vec3_t vertexes[6] = {
		{  0,  0,  0 },
		{ 16,  0,  0 },
		{  0,  0,  0 },
		{  0, 16,  0 },
		{  0,  0,  0 },
		{  0,  0, 16 }
	};
	static color4ub_t colors[6] = {
		{ 255,   0,   0, 255 },
		{ 255,   0,   0, 255 },
		{   0, 255,   0, 255 },
		{   0, 255,   0, 255 },
		{   0,   0, 255, 255 },
		{   0,   0, 255, 255 }
	};
	glRenderState_t state;

	if( mode == TESS_COUNT ) {
		return;
	}

	InitState( &state );

	state.numImages = 1;
	state.image[0] = tr.whiteImage;
	qglLineWidth( 3 );
	SetAttrPointer( &state, AL_VERTEX, 0,
			3, GL_FLOAT, sizeof(vec3_t),
			vertexes );
	SetAttrPointer( &state, AL_COLOR, 0,
			4, GL_UNSIGNED_BYTE, sizeof(color4ub_t),
			colors );
	GL_DrawArrays( &state, GL_LINES, 0, 6 );
	qglLineWidth( 1 );
}

//===========================================================================

/*
====================
RB_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
static void RB_SurfaceEntity( tessMode_t mode, surfaceType_t *surfType ) {
	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPRITE:
		RB_SurfaceSprite( mode );
		break;
	case RT_BEAM:
		RB_SurfaceBeam( mode );
		break;
	case RT_RAIL_CORE:
		RB_SurfaceRailCore( mode );
		break;
	case RT_RAIL_RINGS:
		RB_SurfaceRailRings( mode );
		break;
	case RT_LIGHTNING:
		RB_SurfaceLightningBolt( mode );
		break;
	default:
		RB_SurfaceAxis( mode );
		break;
	}
	return;
}

static void RB_SurfaceMD3Texture( tessMode_t mode, surfaceType_t *surf ) {
	srfMD3Texture_t *srf = (srfMD3Texture_t *)surf;

	if( mode == TESS_COUNT )
		return;

	srf->IBO->next    = tess.firstIBO;
	tess.firstIBO     = srf->IBO;
	tess.dataTexture  = srf->image;
	tess.framesPerRow = srf->framesPerRow;
	tess.scaleX       = srf->scaleX;
	tess.scaleY       = srf->scaleY;
}

static void RB_SurfaceFarPlane( tessMode_t mode, surfaceType_t *surf ) {
	viewParms_t	*p = &backEnd.viewParms;
	vec3_t		mid, left, up;
	color4ub_t	color = { 255,255,255,255 };

	if( mode == TESS_COUNT ) {
		tess.numVertexes += 4;
		tess.numIndexes[tess.indexRange] += 6;
		return;
	}

	VectorMA( p->or.origin, p->zFar, p->or.axis[0], mid );
	VectorScale( p->or.axis[1], p->zFar * tan(p->fovY * M_PI / 360.0f), up );
	VectorScale( p->or.axis[2], p->zFar * tan(p->fovX * M_PI / 360.0f), left );
	RB_AddQuadStampExt( mid, left, up, color, 0.0f, 0.0f, 1.0f, 1.0f );
}

static void RB_SurfaceBad( tessMode_t mode, surfaceType_t *surfType ) {
	ri.Printf( PRINT_ALL, "Bad surface tesselated.\n" );
}

static void RB_SurfaceFlare( tessMode_t mode, surfaceType_t *surface )
{
	srfFlare_t *surf = (srfFlare_t *)surface;

	if( mode && r_flares->integer )
		RB_AddFlare(surf, tess.fogNum, surf->origin, surf->color, surf->normal);
}

static void RB_SurfaceDisplayList( tessMode_t mode, surfaceType_t *surface ) {
	srfDisplayList_t *surf = (srfDisplayList_t *)surface;
	// all apropriate state must be set in RB_BeginSurface
	// this isn't implemented yet...
	if( mode == TESS_COUNT ) {
		return;
	}
	qglCallList( surf->listNum );
}

static void RB_SurfaceIBO( tessMode_t mode, surfaceType_t *surface ) {
	srfIBO_t	*srf = (srfIBO_t *)surface;

	if( mode == TESS_COUNT )
		return;
	
	srf->ibo.next = tess.firstIBO;
	tess.firstIBO = &srf->ibo;
}

static void RB_SurfaceSkip( tessMode_t mode, surfaceType_t *surf ) {
}


void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( tessMode_t, surfaceType_t * ) = {
	RB_SurfaceBad,			// SF_BAD, 
	RB_SurfaceSkip,			// SF_SKIP, 
	RB_SurfaceFace,			// SF_FACE,
	RB_SurfaceGrid,			// SF_GRID,
	RB_SurfaceTriangles,		// SF_TRIANGLES,
	RB_SurfacePolychain,		// SF_POLY,
	RB_SurfaceMesh,			// SF_MD3,
	RB_SurfaceAnim,			// SF_MD4,
#ifdef RAVENMD4
	RB_MDRSurfaceAnim,		// SF_MDR,
#endif
	RB_IQMSurfaceAnim,		// SF_IQM,
	RB_SurfaceFlare,		// SF_FLARE,
	RB_SurfaceEntity,		// SF_ENTITY,
	RB_SurfaceDisplayList,		// SF_DISPLAY_LIST,
	RB_SurfaceIBO,			// SF_IBO,
	RB_SurfaceMD3Texture,		// SF_MD3_TEXTURE,
	RB_SurfaceFarPlane		// SF_FAR_PLANE
};
