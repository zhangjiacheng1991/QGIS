/***************************************************************************
                         qgsmesh3dgeometry_p.cpp
                         -------------------------
    begin                : january 2020
    copyright            : (C) 2020 by Vincent Cloarec
    email                : vcloarec at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmesh3dgeometry_p.h"

#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>

#include <Qt3DRender/qbufferdatagenerator.h>

///@cond PRIVATE

using namespace Qt3DRender;

static QByteArray createPlaneVertexData( const QgsTriangularMesh &mesh,
    const QgsVector3D &origin,
    const QgsRectangle &extent,
    float vertScale )
{
  const int nVerts = mesh.vertices().count();

  QVector<QVector3D> normales = mesh.vertexNormals( vertScale );

  // Populate a buffer with the interleaved per-vertex data with
  // vec3 pos, vec2 texCoord, vec3 normal, vec4 tangent
  const quint32 elementSize = 3 + 2 + 3;
  const quint32 stride = elementSize * sizeof( float );
  QByteArray bufferBytes;
  bufferBytes.resize( stride * nVerts );

  double w = extent.width();
  double h = extent.height() ;
  double x0 = extent.xMinimum();
  double y0 = extent.yMaximum();

  float *fptr = reinterpret_cast<float *>( bufferBytes.data() );

  for ( int i = 0; i < nVerts; i++ )
  {
    const QgsMeshVertex &vert = mesh.vertices().at( i );
    *fptr++ = float( vert.x() - origin.x() );
    *fptr++ = float( vert.z() - origin.z() ) * vertScale ;
    *fptr++ = float( -vert.y() + origin.y() );

    *fptr++ = float( ( vert.x() - x0 ) / w );
    *fptr++ = float( ( y0 - vert.y() ) / h );

    QVector3D normal = normales.at( i ).normalized();
    normal = QVector3D( normal.x(), -normal.y(), normal.z() * vertScale );
    normal.normalized();

    *fptr++ = normal.x();
    *fptr++ = normal.z();
    *fptr++ = normal.y();
  }

  return bufferBytes;
}

static QByteArray createPlaneVertexDatasetData( const QgsTriangularMesh &mesh,
    const QVector<double> verticaleMagnitude,
    const QVector<double> scalarMagnitude,
    const QgsRectangle &extent,
    float vertScale )
{
  const int nVerts = mesh.vertices().count();

  QVector<QVector3D> normales = mesh.vertexNormals( vertScale );

  // Populate a buffer with the interleaved per-vertex data with
  // vec3 pos, vec2 texCoord, vec3 normal, vec4 tangent
  const quint32 elementSize = 3 + 2 + 3 + 1 ;
  const quint32 stride = elementSize * sizeof( float );
  QByteArray bufferBytes;
  bufferBytes.resize( stride * nVerts );

  double w = extent.width();
  double h = extent.height() ;
  double x0 = extent.xMinimum();
  double y0 = extent.yMaximum();

  float *fptr = reinterpret_cast<float *>( bufferBytes.data() );

  for ( int i = 0; i < nVerts; i++ )
  {
    const QgsMeshVertex &vert = mesh.vertices().at( i );
    double vertMag = verticaleMagnitude.at( i );
    double scalarMag = scalarMagnitude.at( i );

    *fptr++ = float( vert.x() );
    *fptr++ = float( vertMag ) * vertScale ;
    *fptr++ = float( -vert.y() );

    *fptr++ = float( ( vert.x() - x0 ) / w );
    *fptr++ = float( ( y0 - vert.y() ) / h );

    QVector3D normal = normales.at( i ).normalized();
    normal = QVector3D( normal.x(), -normal.y(), normal.z() * vertScale );
    normal.normalized();

    *fptr++ = normal.x();
    *fptr++ = normal.z();
    *fptr++ = normal.y();

    *fptr++ = float( scalarMag );
  }

  return bufferBytes;
}

static QByteArray createPlaneIndexData( const QgsTriangularMesh &mesh )
{
  const int faces = mesh.triangles().count();
  const quint32 indices = static_cast<quint32>( 3 * faces );
  Q_ASSERT( indices < std::numeric_limits<quint32>::max() );
  QByteArray indexBytes;
  indexBytes.resize( int( indices * sizeof( quint32 ) ) );
  quint32 *indexPtr = reinterpret_cast<quint32 *>( indexBytes.data() );

  for ( int i = 0; i < faces; ++i )
  {
    const QgsMeshFace &face = mesh.triangles().at( i );
    for ( int i = 0; i < 3; ++i )
      *indexPtr++ = quint32( face.at( i ) );
  }

  return indexBytes;
}

;

//! Generates vertex buffer for Mesh using vertex Z value as verticale magnitude
class MeshPlaneVertexBufferFunctor : public QBufferDataGenerator
{
  public:
    explicit MeshPlaneVertexBufferFunctor( const QgsTriangularMesh &mesh,
                                           const QgsVector3D &origin,
                                           const QgsRectangle &extent,
                                           float vertScale )
      : mMesh( mesh ),
        mOrigin( origin ),
        mExtent( extent ),
        mVertScale( vertScale )

    {}

    QByteArray operator()() final
    {
      return createPlaneVertexData( mMesh, mOrigin, mExtent, mVertScale );
    }

    bool operator ==( const QBufferDataGenerator &other ) const final
    {
      const MeshPlaneVertexBufferFunctor *otherFunctor = functor_cast<MeshPlaneVertexBufferFunctor>( &other );
      if ( otherFunctor != nullptr )
        return ( otherFunctor->mMesh.triangles().count() == mMesh.triangles().count() &&
                 otherFunctor->mExtent == mExtent &&
                 abs( otherFunctor->mVertScale - mVertScale ) < std::numeric_limits<float>::min() );
      return false;
    }

    QT3D_FUNCTOR( MeshPlaneVertexBufferFunctor )

  private:

    QgsTriangularMesh mMesh;
    QgsVector3D mOrigin;
    QgsRectangle mExtent;
    float mVertScale;

};

//! Generates index buffer for Mesh
class MeshPlaneIndexBufferFunctor : public QBufferDataGenerator
{
  public:
    explicit MeshPlaneIndexBufferFunctor( const QgsTriangularMesh &mesh )
      : mMesh( mesh )
    {}

    QByteArray operator()() final
    {
      return createPlaneIndexData( mMesh );
    }

    bool operator ==( const QBufferDataGenerator &other ) const final
    {
      const MeshPlaneIndexBufferFunctor *otherFunctor = functor_cast<MeshPlaneIndexBufferFunctor>( &other );
      if ( otherFunctor != nullptr )
        return ( otherFunctor->mMesh.triangles().count() == mMesh.triangles().count() );
      return false;
    }

    QT3D_FUNCTOR( MeshPlaneIndexBufferFunctor )

  private:
    QgsTriangularMesh mMesh;
};

//! Generates vertex buffer for Mesh using dataset value as verticale magnitude and for color
class MeshPlaneVertexDatasetBufferFunctor : public QBufferDataGenerator
{
  public:
    explicit MeshPlaneVertexDatasetBufferFunctor( const QgsTriangularMesh &mesh,
        const QVector<double> verticaleMagnitude,
        const QVector<double> scalarMagnitude,
        const QgsRectangle &extent,
        float vertScale )
      : mMesh( mesh ),
        mVerticaleMagnitude( verticaleMagnitude ),
        mScalarMagnitude( scalarMagnitude ),
        mExtent( extent ),
        mVertScale( vertScale )
    {}

    QByteArray operator()() final
    {
      return createPlaneVertexDatasetData( mMesh,
                                           mVerticaleMagnitude,
                                           mScalarMagnitude,
                                           mExtent,
                                           mVertScale );
    }

    bool operator ==( const QBufferDataGenerator &other ) const final
    {
      const MeshPlaneVertexDatasetBufferFunctor *otherFunctor = functor_cast<MeshPlaneVertexDatasetBufferFunctor>( &other );
      if ( otherFunctor != nullptr )
        return ( otherFunctor->mMesh.triangles().count() == mMesh.triangles().count() &&
                 otherFunctor->mVerticaleMagnitude.count() == mVerticaleMagnitude.count() &&
                 otherFunctor->mScalarMagnitude.count() == mScalarMagnitude.count() &&
                 otherFunctor->mExtent == mExtent &&
                 abs( otherFunctor->mVertScale - mVertScale ) < std::numeric_limits<float>::min() );
      return false;
    }

    QT3D_FUNCTOR( MeshPlaneVertexBufferFunctor )

  private:
    QgsTriangularMesh mMesh;
    QVector<double> mVerticaleMagnitude;
    QVector<double> mScalarMagnitude;
    QgsRectangle mExtent;
    float mVertScale;

};

QgsMesh3dGeometry::QgsMesh3dGeometry(
  const QgsTriangularMesh &mesh,
  const QgsVector3D &origin,
  const QgsRectangle &extent,
  float verticaleScale,
  QgsMesh3dGeometry::QNode *parent ):
  QGeometry( parent ),
  mTriangularMesh( mesh ),
  mOrigin( origin ),
  mExtent( extent ),
  mVertScale( verticaleScale )
{
  init();
}

void QgsMesh3dGeometry::init()
{
  mPositionAttribute = new QAttribute( this );
  mNormalAttribute = new QAttribute( this );
  mTexCoordAttribute = new QAttribute( this );
  mIndexAttribute = new QAttribute( this );

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
  mVertexBuffer = new Qt3DRender::QBuffer( Qt3DRender::QBuffer::VertexBuffer, this );
  mIndexBuffer = new Qt3DRender::QBuffer( Qt3DRender::QBuffer::IndexBuffer, this );
#else
  mVertexBuffer = new Qt3DRender::QBuffer( this );
  mIndexBuffer = new Qt3DRender::QBuffer( this );
#endif

  const int stride = ( 3 /*position*/ +
                       2 /*texture*/  +
                       3 /*normale*/ ) * sizeof( float );

  const uint nVerts = uint( mTriangularMesh.vertices().count() );

  mPositionAttribute->setName( QAttribute::defaultPositionAttributeName() );
  mPositionAttribute->setVertexBaseType( QAttribute::Float );
  mPositionAttribute->setVertexSize( 3 );
  mPositionAttribute->setAttributeType( QAttribute::VertexAttribute );
  mPositionAttribute->setBuffer( mVertexBuffer );
  mPositionAttribute->setByteStride( stride );
  mPositionAttribute->setCount( nVerts );

  mTexCoordAttribute->setName( QAttribute::defaultTextureCoordinateAttributeName() );
  mTexCoordAttribute->setVertexBaseType( QAttribute::Float );
  mTexCoordAttribute->setVertexSize( 2 );
  mTexCoordAttribute->setAttributeType( QAttribute::VertexAttribute );
  mTexCoordAttribute->setBuffer( mVertexBuffer );
  mTexCoordAttribute->setByteStride( stride );
  mTexCoordAttribute->setByteOffset( 3 * sizeof( float ) );
  mTexCoordAttribute->setCount( nVerts );

  mNormalAttribute->setName( QAttribute::defaultNormalAttributeName() );
  mNormalAttribute->setVertexBaseType( QAttribute::Float );
  mNormalAttribute->setVertexSize( 3 );
  mNormalAttribute->setAttributeType( QAttribute::VertexAttribute );
  mNormalAttribute->setBuffer( mVertexBuffer );
  mNormalAttribute->setByteStride( stride );
  mNormalAttribute->setByteOffset( 5 * sizeof( float ) );
  mNormalAttribute->setCount( nVerts );

  mIndexAttribute->setAttributeType( QAttribute::IndexAttribute );
  mIndexAttribute->setVertexBaseType( QAttribute::UnsignedInt );
  mIndexAttribute->setBuffer( mIndexBuffer );

  // Each primitive has 3 vertives
  mIndexAttribute->setCount( uint( mTriangularMesh.triangles().count() ) * 3 );

  mVertexBuffer->setData( MeshPlaneVertexBufferFunctor( mTriangularMesh, mOrigin, mExtent, mVertScale )() );
  mIndexBuffer->setData( MeshPlaneIndexBufferFunctor( mTriangularMesh )() );

  addAttribute( mPositionAttribute );
  addAttribute( mTexCoordAttribute );
  addAttribute( mNormalAttribute );
  addAttribute( mIndexAttribute );
}

