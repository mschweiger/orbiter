// Copyright (c) Martin Schweiger
// Licensed under the MIT License

//-----------------------------------------------------------------------------
// File: D3DMath.cpp
//
// Desc: Shortcut macros and functions for using DX objects
//
//
// Copyright (c) 1997-1998 Microsoft Corporation. All rights reserved
//-----------------------------------------------------------------------------

#define __D3DMATH_CPP
#define D3D_OVERLOADS
#define STRICT
#include <math.h>
#include <stdio.h>
#include "D3dmath.h"

VOID VMAT_rotx (D3DMATRIX &a, double r)
{
	double sinr = sin(r), cosr = cos(r);
	ZeroMemory (&a, sizeof (D3DMATRIX));
	a._22 = a._33 = (FLOAT)cosr;
	a._23 = -(a._32 = (FLOAT)sinr);
	a._11 = a._44 = 1.0f;
}

VOID VMAT_roty (D3DMATRIX &a, double r)
{
	double sinr = sin(r), cosr = cos(r);
	ZeroMemory (&a, sizeof (D3DMATRIX));
	a._11 = a._33 = (FLOAT)cosr;
	a._31 = -(a._13 = (FLOAT)sinr);
	a._22 = a._44 = 1.0f;
}

// Create a rotation matrix from a rotation axis and angle
VOID VMAT_rotation_from_axis (const D3DVECTOR &axis, D3DVALUE angle, D3DMATRIX &R)
{
	// Calculate quaternion
	angle *= 0.5f;
	D3DVALUE w = cosf(angle), sina = sinf(angle);
	D3DVALUE x = sina * axis.x;
	D3DVALUE y = sina * axis.y;
	D3DVALUE z = sina * axis.z;

	// Rotation matrix
	D3DVALUE xx = x*x, yy = y*y, zz = z*z;
	D3DVALUE xy = x*y, xz = x*z, yz = y*z;
	D3DVALUE wx = w*x, wy = w*y, wz = w*z;

	R._11 = 1 - 2 * (yy+zz);
	R._12 =     2 * (xy+wz);
	R._13 =     2 * (xz-wy);
	R._21 =     2 * (xy-wz);
	R._22 = 1 - 2 * (xx+zz);
	R._23 =     2 * (yz+wx);
	R._31 =     2 * (xz+wy);
	R._32 =     2 * (yz-wx);
	R._33 = 1 - 2 * (xx+yy);

	R._14 = R._24 = R._34 = R._41 = R._42 = R._43 = 0.0f;
	R._44 = 1.0f;
}

void D3DMathSetup ()
{
	VMAT_identity (Identity);
}

//-----------------------------------------------------------------------------
// Name: D3DMath_MatrixMultiply()
// Desc: Does the matrix operation: [Q] = [A] * [B].
//-----------------------------------------------------------------------------
VOID D3DMath_MatrixMultiply( D3DMATRIX& q, D3DMATRIX& a, D3DMATRIX& b )
{
    FLOAT* pA = (FLOAT*)&a;
    FLOAT* pB = (FLOAT*)&b;
    FLOAT  pM[16];

    ZeroMemory( pM, sizeof(D3DMATRIX) );

    for( WORD i=0; i<4; i++ ) 
        for( WORD j=0; j<4; j++ ) 
            for( WORD k=0; k<4; k++ ) 
                pM[4*i+j] += pA[4*k+j] * pB[4*i+k];

    memcpy( &q, pM, sizeof(D3DMATRIX) );
}




//-----------------------------------------------------------------------------
// Name: D3DMath_MatrixInvert()
// Desc: Does the matrix operation: [Q] = inv[A]. Note: this function only
//       works for matrices with [0 0 0 1] for the 4th column.
//-----------------------------------------------------------------------------
HRESULT D3DMath_MatrixInvert( D3DMATRIX& q, D3DMATRIX& a )
{
    if( fabs(a._44 - 1.0f) > .001f)
        return E_INVALIDARG;
    if( fabs(a._14) > .001f || fabs(a._24) > .001f || fabs(a._34) > .001f )
        return E_INVALIDARG;

    FLOAT fDetInv = 1.0f / ( a._11 * ( a._22 * a._33 - a._23 * a._32 ) -
                             a._12 * ( a._21 * a._33 - a._23 * a._31 ) +
                             a._13 * ( a._21 * a._32 - a._22 * a._31 ) );

    q._11 =  fDetInv * ( a._22 * a._33 - a._23 * a._32 );
    q._12 = -fDetInv * ( a._12 * a._33 - a._13 * a._32 );
    q._13 =  fDetInv * ( a._12 * a._23 - a._13 * a._22 );
    q._14 = 0.0f;

    q._21 = -fDetInv * ( a._21 * a._33 - a._23 * a._31 );
    q._22 =  fDetInv * ( a._11 * a._33 - a._13 * a._31 );
    q._23 = -fDetInv * ( a._11 * a._23 - a._13 * a._21 );
    q._24 = 0.0f;

    q._31 =  fDetInv * ( a._21 * a._32 - a._22 * a._31 );
    q._32 = -fDetInv * ( a._11 * a._32 - a._12 * a._31 );
    q._33 =  fDetInv * ( a._11 * a._22 - a._12 * a._21 );
    q._34 = 0.0f;

    q._41 = -( a._41 * q._11 + a._42 * q._21 + a._43 * q._31 );
    q._42 = -( a._41 * q._12 + a._42 * q._22 + a._43 * q._32 );
    q._43 = -( a._41 * q._13 + a._42 * q._23 + a._43 * q._33 );
    q._44 = 1.0f;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: D3DMath_VectorMatrixMultiply()
// Desc: Multiplies a vector by a matrix
//-----------------------------------------------------------------------------
HRESULT D3DMath_VectorMatrixMultiply( D3DVECTOR& vDest, const D3DVECTOR& vSrc,
                                      const D3DMATRIX& mat)
{
    FLOAT x = vSrc.x*mat._11 + vSrc.y*mat._21 + vSrc.z* mat._31 + mat._41;
    FLOAT y = vSrc.x*mat._12 + vSrc.y*mat._22 + vSrc.z* mat._32 + mat._42;
    FLOAT z = vSrc.x*mat._13 + vSrc.y*mat._23 + vSrc.z* mat._33 + mat._43;
    FLOAT w = vSrc.x*mat._14 + vSrc.y*mat._24 + vSrc.z* mat._34 + mat._44;
    
    if( fabs( w ) < g_EPSILON )
        return E_INVALIDARG;

    vDest.x = x/w;
    vDest.y = y/w;
    vDest.z = z/w;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: D3DMath_VectorTMatrixMultiply()
// Desc: Multiplies a vector by the transpose of a matrix
//-----------------------------------------------------------------------------
HRESULT D3DMath_VectorTMatrixMultiply( D3DVECTOR& vDest, const D3DVECTOR& vSrc,
                                      const D3DMATRIX& mat)
{
    FLOAT x = vSrc.x*mat._11 + vSrc.y*mat._12 + vSrc.z* mat._13 + mat._14;
    FLOAT y = vSrc.x*mat._21 + vSrc.y*mat._22 + vSrc.z* mat._23 + mat._24;
    FLOAT z = vSrc.x*mat._31 + vSrc.y*mat._32 + vSrc.z* mat._33 + mat._34;
    FLOAT w = vSrc.x*mat._41 + vSrc.y*mat._42 + vSrc.z* mat._43 + mat._44;
    
    if( fabs( w ) < g_EPSILON )
        return E_INVALIDARG;

    vDest.x = x/w;
    vDest.y = y/w;
    vDest.z = z/w;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: D3DMath_VertexMatrixMultiply()
// Desc: Multiplies a vertex by a matrix
//-----------------------------------------------------------------------------
HRESULT D3DMath_VertexMatrixMultiply( D3DVERTEX& vDest, const D3DVERTEX& vSrc,
                                      const D3DMATRIX& mat )
{
    HRESULT    hr;
    D3DVECTOR* pSrcVec  = (D3DVECTOR*)&vSrc.x;
    D3DVECTOR* pDestVec = (D3DVECTOR*)&vDest.x;

    if( SUCCEEDED( hr = D3DMath_VectorMatrixMultiply( *pDestVec, *pSrcVec,
                                                      mat ) ) )
    {
        pSrcVec  = (D3DVECTOR*)&vSrc.nx;
        pDestVec = (D3DVECTOR*)&vDest.nx;
        hr = D3DMath_VectorMatrixMultiply( *pDestVec, *pSrcVec, mat );
    }
    return hr;
}




//-----------------------------------------------------------------------------
// Name: D3DMath_QuaternionFromRotation()
// Desc: Converts a normalized axis and angle to a unit quaternion.
//-----------------------------------------------------------------------------
VOID D3DMath_QuaternionFromRotation( FLOAT& x, FLOAT& y, FLOAT& z, FLOAT& w,
                                     D3DVECTOR& v, FLOAT fTheta )
{
    x = (FLOAT)sin(fTheta/2) * v.x;
    y = (FLOAT)sin(fTheta/2) * v.y;
    z = (FLOAT)sin(fTheta/2) * v.z;
    w = (FLOAT)cos(fTheta/2);
}




//-----------------------------------------------------------------------------
// Name: D3DMath_RotationFromQuaternion()
// Desc: Converts a normalized axis and angle to a unit quaternion.
//-----------------------------------------------------------------------------
VOID D3DMath_RotationFromQuaternion( D3DVECTOR& v, FLOAT& fTheta,
                                     FLOAT x, FLOAT y, FLOAT z, FLOAT w )
                                      
{
    fTheta = (FLOAT)( acos(w) * 2 );
    v.x    = (FLOAT)( x / sin(fTheta/2) );
    v.y    = (FLOAT)( y / sin(fTheta/2) );
    v.z    = (FLOAT)( z / sin(fTheta/2) );
}




//-----------------------------------------------------------------------------
// Name: D3DMath_QuaternionFromAngles()
// Desc: Converts euler angles to a unit quaternion.
//-----------------------------------------------------------------------------
VOID D3DMath_QuaternionFromAngles( FLOAT& x, FLOAT& y, FLOAT& z, FLOAT& w,
                                   FLOAT fYaw, FLOAT fPitch, FLOAT fRoll )
                                        
{
    FLOAT fSinYaw   = (FLOAT)sin(fYaw/2);
    FLOAT fSinPitch = (FLOAT)sin(fPitch/2);
    FLOAT fSinRoll  = (FLOAT)sin(fRoll/2);
    FLOAT fCosYaw   = (FLOAT)cos(fYaw/2);
    FLOAT fCosPitch = (FLOAT)cos(fPitch/2);
    FLOAT fCosRoll  = (FLOAT)cos(fRoll/2);

    x = fSinRoll * fCosPitch * fCosYaw - fCosRoll * fSinPitch * fSinYaw;
    y = fCosRoll * fSinPitch * fCosYaw + fSinRoll * fCosPitch * fSinYaw;
    z = fCosRoll * fCosPitch * fSinYaw - fSinRoll * fSinPitch * fCosYaw;
    w = fCosRoll * fCosPitch * fCosYaw + fSinRoll * fSinPitch * fSinYaw;
}




//-----------------------------------------------------------------------------
// Name: D3DMath_MatrixFromQuaternion()
// Desc: Converts a unit quaternion into a rotation matrix.
//-----------------------------------------------------------------------------
VOID D3DMath_MatrixFromQuaternion( D3DMATRIX& mat, FLOAT x, FLOAT y, FLOAT z,
                                   FLOAT w )
{
    FLOAT xx = x*x; FLOAT yy = y*y; FLOAT zz = z*z;
    FLOAT xy = x*y; FLOAT xz = x*z; FLOAT yz = y*z;
    FLOAT wx = w*x; FLOAT wy = w*y; FLOAT wz = w*z;
    
    mat._11 = 1 - 2 * ( yy + zz ); 
    mat._12 =     2 * ( xy - wz );
    mat._13 =     2 * ( xz + wy );

    mat._21 =     2 * ( xy + wz );
    mat._22 = 1 - 2 * ( xx + zz );
    mat._23 =     2 * ( yz - wx );

    mat._31 =     2 * ( xz - wy );
    mat._32 =     2 * ( yz + wx );
    mat._33 = 1 - 2 * ( xx + yy );

    mat._14 = mat._24 = mat._34 = 0.0f;
    mat._41 = mat._42 = mat._43 = 0.0f;
    mat._44 = 1.0f;
}




//-----------------------------------------------------------------------------
// Name: D3DMath_QuaternionFromMatrix()
// Desc: Converts a rotation matrix into a unit quaternion.
//-----------------------------------------------------------------------------
VOID D3DMath_QuaternionFromMatrix( FLOAT& x, FLOAT& y, FLOAT& z, FLOAT& w,
                                   D3DMATRIX& mat )
{
    if( mat._11 + mat._22 + mat._33 > 0.0f )
    {
        FLOAT s = (FLOAT)sqrt( mat._11 + mat._22 + mat._33 + mat._44 );

        x = (mat._23-mat._32) / (2*s);
        y = (mat._31-mat._13) / (2*s);
        z = (mat._12-mat._21) / (2*s);
        w = 0.5f * s;
    }
    else
    {


    }
    FLOAT xx = x*x; FLOAT yy = y*y; FLOAT zz = z*z;
    FLOAT xy = x*y; FLOAT xz = x*z; FLOAT yz = y*z;
    FLOAT wx = w*x; FLOAT wy = w*y; FLOAT wz = w*z;
    
    mat._11 = 1 - 2 * ( yy + zz ); 
    mat._12 =     2 * ( xy - wz );
    mat._13 =     2 * ( xz + wy );

    mat._21 =     2 * ( xy + wz );
    mat._22 = 1 - 2 * ( xx + zz );
    mat._23 =     2 * ( yz - wx );

    mat._31 =     2 * ( xz - wy );
    mat._32 =     2 * ( yz + wx );
    mat._33 = 1 - 2 * ( xx + yy );

    mat._14 = mat._24 = mat._34 = 0.0f;
    mat._41 = mat._42 = mat._43 = 0.0f;
    mat._44 = 1.0f;
}




//-----------------------------------------------------------------------------
// Name: D3DMath_QuaternionMultiply()
// Desc: Mulitples two quaternions together as in {Q} = {A} * {B}.
//-----------------------------------------------------------------------------
VOID D3DMath_QuaternionMultiply( FLOAT& Qx, FLOAT& Qy, FLOAT& Qz, FLOAT& Qw,
                                  FLOAT Ax, FLOAT Ay, FLOAT Az, FLOAT Aw,
                                  FLOAT Bx, FLOAT By, FLOAT Bz, FLOAT Bw )
{
    FLOAT Dx = Bw*Ax + Bx*Aw + By*Az + Bz*Ay;
    FLOAT Dy = Bw*Ay + By*Aw + Bz*Ax + Bx*Az;
    FLOAT Dz = Bw*Az + Bz*Aw + Bx*Ay + By*Ax;
    FLOAT Dw = Bw*Aw + Bx*Ax + By*Ay + Bz*Az;

    Qx = Dx; Qy = Dy; Qz = Dz; Qw = Dw;
}




//-----------------------------------------------------------------------------
// Name: D3DMath_SlerpQuaternions()
// Desc: Compute a quaternion which is the spherical linear interpolation
//       between two other quaternions by dvFraction.
//-----------------------------------------------------------------------------
VOID D3DMath_QuaternionSlerp( FLOAT& Qx, FLOAT& Qy, FLOAT& Qz, FLOAT& Qw,
                              FLOAT Ax, FLOAT Ay, FLOAT Az, FLOAT Aw,
                              FLOAT Bx, FLOAT By, FLOAT Bz, FLOAT Bw,
                              FLOAT fAlpha )
{
    FLOAT fScale1;
    FLOAT fScale2;

    // Compute dot product, aka cos(theta):
    FLOAT fCosTheta = Ax*Bx + Ay*By + Az*Bz + Aw*Bw;

    if( fCosTheta < 0.0f )
    {
        // Flip start quaternion
        Ax = -Ax; Ay = -Ay; Ax = -Az; Aw = -Aw;
        fCosTheta = -fCosTheta;
    }

    if( fCosTheta + 1.0f > 0.05f )
    {
        // If the quaternions are close, use linear interploation
        if( 1.0f - fCosTheta < 0.05f )
        {
            fScale1 = 1.0f - fAlpha;
            fScale2 = fAlpha;
        }
        else // Otherwise, do spherical interpolation
        {
            FLOAT fTheta    = (FLOAT)acos( fCosTheta );
            FLOAT fSinTheta = (FLOAT)sin( fTheta );
            
            fScale1 = (FLOAT)sin( fTheta * (1.0f-fAlpha) ) / fSinTheta;
            fScale2 = (FLOAT)sin( fTheta * fAlpha ) / fSinTheta;
        }
    }
    else
    {
        Bx = -Ay;
        By =  Ax;
        Bz = -Aw;
        Bw =  Az;
        fScale1 = (FLOAT)sin( g_PI * (0.5f - fAlpha) );
        fScale2 = (FLOAT)sin( g_PI * fAlpha );
    }

    Qx = fScale1 * Ax + fScale2 * Bx;
    Qy = fScale1 * Ay + fScale2 * By;
    Qz = fScale1 * Az + fScale2 * Bz;
    Qw = fScale1 * Aw + fScale2 * Bw;
}



