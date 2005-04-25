/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2005 Robert Osfield 
 * Copyright (C) 2003-2005 3Dlabs Inc. Ltd.
 *
 * This application is open source and may be redistributed and/or modified   
 * freely and without restriction, both in commericial and non commericial applications,
 * as long as this copyright notice is maintained.
 * 
 * This application is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

/* file:	examples/osgshaders/GL2Scene.cpp
 * author:	Mike Weiblen 2005-04-15
 *
 * Compose a scene of several instances of a model, with a different
 * OpenGL Shading Language shader applied to each.
 *
 * See http://www.3dlabs.com/opengl2/ for more information regarding
 * the OpenGL Shading Language.
*/

#include <osg/ShapeDrawable>
#include <osg/PositionAttitudeTransform>
#include <osg/Geode>
#include <osg/Node>
#include <osg/Material>
#include <osg/Notify>
#include <osg/Vec3>
#include <osg/Texture1D>
#include <osg/Texture2D>
#include <osg/Texture3D>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgUtil/Optimizer>

#include <osg/Program>
#include <osg/Shader>
#include <osg/Uniform>

#include <iostream>

#include "GL2Scene.h"
#include "Noise.h"

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

static osg::Image*
make3DNoiseImage(int texSize)
{
    osg::Image* image = new osg::Image;
    image->setImage(texSize, texSize, texSize,
	    4, GL_RGBA, GL_UNSIGNED_BYTE,
	    new unsigned char[4 * texSize * texSize * texSize],
	    osg::Image::USE_NEW_DELETE);

    const int startFrequency = 4;
    const int numOctaves = 4;

    int f, i, j, k, inc;
    double ni[3];
    double inci, incj, inck;
    int frequency = startFrequency;
    GLubyte *ptr;
    double amp = 0.5;

    osg::notify(osg::INFO) << "creating 3D noise texture... ";

    for (f = 0, inc = 0; f < numOctaves; ++f, frequency *= 2, ++inc, amp *= 0.5)
    {
	SetNoiseFrequency(frequency);
	ptr = image->data();
	ni[0] = ni[1] = ni[2] = 0;

	inci = 1.0 / (texSize / frequency);
	for (i = 0; i < texSize; ++i, ni[0] += inci)
	{
	    incj = 1.0 / (texSize / frequency);
	    for (j = 0; j < texSize; ++j, ni[1] += incj)
	    {
		inck = 1.0 / (texSize / frequency);
		for (k = 0; k < texSize; ++k, ni[2] += inck, ptr += 4)
		{
		    *(ptr+inc) = (GLubyte) (((noise3(ni) + 1.0) * amp) * 128.0);
		}
	    }
	}
    }

    osg::notify(osg::INFO) << "DONE" << std::endl;
    return image;	
}

static osg::Texture3D*
make3DNoiseTexture(int texSize )
{
    osg::Texture3D* noiseTexture = new osg::Texture3D;
    noiseTexture->setFilter(osg::Texture3D::MIN_FILTER, osg::Texture3D::LINEAR);
    noiseTexture->setFilter(osg::Texture3D::MAG_FILTER, osg::Texture3D::LINEAR);
    noiseTexture->setWrap(osg::Texture3D::WRAP_S, osg::Texture3D::REPEAT);
    noiseTexture->setWrap(osg::Texture3D::WRAP_T, osg::Texture3D::REPEAT);
    noiseTexture->setWrap(osg::Texture3D::WRAP_R, osg::Texture3D::REPEAT);
    noiseTexture->setImage( make3DNoiseImage(texSize) );
    return noiseTexture;
}

///////////////////////////////////////////////////////////////////////////

static osg::Image*
make1DSineImage( int texSize )
{
    const float PI = 3.1415927;

    osg::Image* image = new osg::Image;
    image->setImage(texSize, 1, 1,
	    4, GL_RGBA, GL_UNSIGNED_BYTE,
	    new unsigned char[4 * texSize],
	    osg::Image::USE_NEW_DELETE);

    GLubyte* ptr = image->data();
    float inc = 2. * PI / (float)texSize;
    for(int i = 0; i < texSize; i++)
    {
	*ptr++ = (GLubyte)((sinf(i * inc) * 0.5 + 0.5) * 255.);
	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 1;
    }
    return image;	
}

static osg::Texture1D*
make1DSineTexture( int texSize )
{
    osg::Texture1D* sineTexture = new osg::Texture1D;
    sineTexture->setWrap(osg::Texture1D::WRAP_S, osg::Texture1D::REPEAT);
    sineTexture->setFilter(osg::Texture1D::MIN_FILTER, osg::Texture1D::LINEAR);
    sineTexture->setFilter(osg::Texture1D::MAG_FILTER, osg::Texture1D::LINEAR);
    sineTexture->setImage( make1DSineImage(texSize) );
    return sineTexture;
}

///////////////////////////////////////////////////////////////////////////
// OpenGL Shading Language source code for the "microshader" example,
// which simply colors a fragment based on its location.

static const char *microshaderVertSource = {
    "varying vec4 color;"
    "void main(void)"
    "{"
	"color = gl_Vertex;"
	"gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
    "}"
};

static const char *microshaderFragSource = {
    "varying vec4 color;"
    "void main(void)"
    "{"
	"gl_FragColor = clamp( color, 0.0, 1.0 );"
    "}"
};

///////////////////////////////////////////////////////////////////////////

static osg::ref_ptr<osg::Group> rootNode;

// Create some geometry upon which to render GL2 shaders.
static osg::Geode*
CreateModel()
{
    osg::Geode* geode = new osg::Geode();
    geode->addDrawable(new osg::ShapeDrawable(new osg::Sphere(osg::Vec3(0.0f,0.0f,0.0f),1.0f)));
    geode->addDrawable(new osg::ShapeDrawable(new osg::Cone(osg::Vec3(2.2f,0.0f,-0.4f),0.9f,1.8f)));
    geode->addDrawable(new osg::ShapeDrawable(new osg::Cylinder(osg::Vec3(4.4f,0.0f,0.0f),1.0f,1.4f)));
    return geode;
}

// Add a reference to the masterModel at the specified translation, and
// return its StateSet so we can easily attach StateAttributes.
static osg::StateSet*
ModelInstance()
{
    static float zvalue = 0.0f;
    static osg::Node* masterModel = CreateModel();

    osg::PositionAttitudeTransform* xform = new osg::PositionAttitudeTransform();
    xform->setPosition(osg::Vec3( 0.0f, -1.0f, zvalue ));
    zvalue = zvalue + 2.2f;
    xform->addChild(masterModel);
    rootNode->addChild(xform);
    return xform->getOrCreateStateSet();
}

// load source from a file.
static void
LoadShaderSource( osg::Shader* shader, const std::string& fileName )
{
    std::string fqFileName = osgDB::findDataFile(fileName);
    if( fqFileName.length() != 0 )
    {
	shader->loadShaderSourceFromFile( fqFileName.c_str() );
    }
    else
    {
	osg::notify(osg::WARN) << "File \"" << fileName << "\" not found." << std::endl;
    }
}


///////////////////////////////////////////////////////////////////////////
// rude but convenient globals

static osg::Program* BlockyProgram;
static osg::Shader*  BlockyVertObj;
static osg::Shader*  BlockyFragObj;

static osg::Program* ErodedProgram;
static osg::Shader*  ErodedVertObj;
static osg::Shader*  ErodedFragObj;

static osg::Program* MarbleProgram;
static osg::Shader*  MarbleVertObj;
static osg::Shader*  MarbleFragObj;


///////////////////////////////////////////////////////////////////////////
// for demo simplicity, this one callback animates all the shaders, instancing
// for each uniform but with a specific operation each time.

class AnimateCallback: public osg::Uniform::Callback
{
    public:
    
        enum Operation
        {
            OFFSET,
            SIN,
            COLOR1,
            COLOR2            
        };
    
	AnimateCallback(Operation op) : _enabled(true),_operation(op) {}

	virtual void operator() ( osg::Uniform* uniform, osg::NodeVisitor* nv )
	{
	    if( _enabled )
	    {
		float angle = 2.0 * nv->getFrameStamp()->getReferenceTime();
		float sine = sinf( angle );	// -1 -> 1
		float v01 = 0.5f * sine + 0.5f;	//  0 -> 1
		float v10 = 1.0f - v01;		//  1 -> 0
                switch(_operation)
                {
                    case OFFSET : uniform->set( osg::Vec3(0.505f, 0.8f*v01, 0.0f) ); break;
                    case SIN : uniform->set( sine ); break;
                    case COLOR1 : uniform->set( osg::Vec3(v10, 0.0f, 0.0f) ); break;
		    case COLOR2 : uniform->set( osg::Vec3(v01, v01, v10) ); break;
                }
	    }
	}

    private:
	bool _enabled;
        Operation _operation;
};

///////////////////////////////////////////////////////////////////////////
// Compose a scenegraph with examples of GL2 shaders

#define TEXUNIT_SINE	1
#define TEXUNIT_NOISE	2

osg::ref_ptr<osg::Group>
GL2Scene::buildScene()
{
    osg::Texture3D* noiseTexture = make3DNoiseTexture( 32 /*128*/ );
    osg::Texture1D* sineTexture = make1DSineTexture( 32 /*1024*/ );

    // the root of our scenegraph.
    rootNode = new osg::Group;
    //rootNode->setUpdateCallback( new AnimateCallback );

    // attach some Uniforms to the root, to be inherited by Programs.
    {
	osg::Uniform* OffsetUniform = new osg::Uniform( "Offset", osg::Vec3(0.0f, 0.0f, 0.0f) );
	osg::Uniform* SineUniform   = new osg::Uniform( "Sine", 0.0f );
	osg::Uniform* Color1Uniform = new osg::Uniform( "Color1", osg::Vec3(0.0f, 0.0f, 0.0f) );
	osg::Uniform* Color2Uniform = new osg::Uniform( "Color2", osg::Vec3(0.0f, 0.0f, 0.0f) );

        OffsetUniform->setUpdateCallback(new AnimateCallback(AnimateCallback::OFFSET));
        SineUniform->setUpdateCallback(new AnimateCallback(AnimateCallback::SIN));
        Color1Uniform->setUpdateCallback(new AnimateCallback(AnimateCallback::COLOR1));
        Color2Uniform->setUpdateCallback(new AnimateCallback(AnimateCallback::COLOR2));

	osg::StateSet* ss = rootNode->getOrCreateStateSet();
	ss->addUniform( OffsetUniform );
	ss->addUniform( SineUniform );
	ss->addUniform( Color1Uniform );
	ss->addUniform( Color2Uniform );
        
        
        //ss->setUpdateCallback(new AnimateCallback2);
    }

    // the simple Microshader (its source appears earlier in this file)
    {
	osg::StateSet* ss = ModelInstance();
	osg::Program* program = new osg::Program;
	program->setName( "microshader" );
	_programList.push_back( program );
	program->addShader( new osg::Shader( osg::Shader::VERTEX, microshaderVertSource ) );
	program->addShader( new osg::Shader( osg::Shader::FRAGMENT, microshaderFragSource ) );
	ss->setAttributeAndModes( program, osg::StateAttribute::ON );
    }

    // the "blocky" shader, a simple animation test
    {
	osg::StateSet* ss = ModelInstance();
	BlockyProgram = new osg::Program;
	BlockyProgram->setName( "blocky" );
	_programList.push_back( BlockyProgram );
	BlockyVertObj = new osg::Shader( osg::Shader::VERTEX );
	BlockyFragObj = new osg::Shader( osg::Shader::FRAGMENT );
	BlockyProgram->addShader( BlockyFragObj );
	BlockyProgram->addShader( BlockyVertObj );
	ss->setAttributeAndModes(BlockyProgram, osg::StateAttribute::ON);
    }

    // the "eroded" shader, uses a noise texture to discard fragments
    {
	osg::StateSet* ss = ModelInstance();
	ss->setTextureAttribute(TEXUNIT_NOISE, noiseTexture);
	ErodedProgram = new osg::Program;
	ErodedProgram->setName( "eroded" );
	_programList.push_back( ErodedProgram );
	ErodedVertObj = new osg::Shader( osg::Shader::VERTEX );
	ErodedFragObj = new osg::Shader( osg::Shader::FRAGMENT );
	ErodedProgram->addShader( ErodedFragObj );
	ErodedProgram->addShader( ErodedVertObj );
	ss->setAttributeAndModes(ErodedProgram, osg::StateAttribute::ON);

	ss->addUniform( new osg::Uniform("LightPosition", osg::Vec3(0.0f, 0.0f, 4.0f)) );
	ss->addUniform( new osg::Uniform("Scale", 1.0f) );
	ss->addUniform( new osg::Uniform("sampler3d", TEXUNIT_NOISE) );
    }

    // the "marble" shader, uses two textures
    {
	osg::StateSet* ss = ModelInstance();
	ss->setTextureAttribute(TEXUNIT_NOISE, noiseTexture);
	ss->setTextureAttribute(TEXUNIT_SINE, sineTexture);
	MarbleProgram = new osg::Program;
	MarbleProgram->setName( "marble" );
	_programList.push_back( MarbleProgram );
	MarbleVertObj = new osg::Shader( osg::Shader::VERTEX );
	MarbleFragObj = new osg::Shader( osg::Shader::FRAGMENT );
	MarbleProgram->addShader( MarbleFragObj );
	MarbleProgram->addShader( MarbleVertObj );
	ss->setAttributeAndModes(MarbleProgram, osg::StateAttribute::ON);

	ss->addUniform( new osg::Uniform("NoiseTex", TEXUNIT_NOISE) );
	ss->addUniform( new osg::Uniform("SineTex", TEXUNIT_SINE) );
    }

#ifdef INTERNAL_3DLABS //[
    // regular GL 1.x texturing for comparison.
    osg::StateSet* ss = ModelInstance();
    osg::Texture2D* tex0 = new osg::Texture2D;
    tex0->setImage( osgDB::readImageFile( "images/3dl-ge100.png" ) );
    ss->setTextureAttributeAndModes(0, tex0, osg::StateAttribute::ON);
#endif //]

    reloadShaderSource();

#ifdef INTERNAL_3DLABS //[
    // add logo overlays
    rootNode->addChild( osgDB::readNodeFile( "3dl_ogl.logo" ) );
#endif //]

    return rootNode;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

GL2Scene::GL2Scene()
{
    _rootNode = buildScene();
    _shadersEnabled = true;
}

GL2Scene::~GL2Scene()
{
}

void
GL2Scene::reloadShaderSource()
{
    osg::notify(osg::INFO) << "reloadShaderSource()" << std::endl;

    LoadShaderSource( BlockyVertObj, "shaders/blocky.vert" );
    LoadShaderSource( BlockyFragObj, "shaders/blocky.frag" );

    LoadShaderSource( ErodedVertObj, "shaders/eroded.vert" );
    LoadShaderSource( ErodedFragObj, "shaders/eroded.frag" );

    LoadShaderSource( MarbleVertObj, "shaders/marble.vert" );
    LoadShaderSource( MarbleFragObj, "shaders/marble.frag" );
}


// mew 2003-09-19 : TODO Need to revisit how to better control
// osg::Program enable state in OSG core.  glProgram are
// different enough from other GL state that StateSet::setAttributeAndModes()
// doesn't fit well, so came up with a local implementation.
void
GL2Scene::toggleShaderEnable()
{
    _shadersEnabled = ! _shadersEnabled;
    osg::notify(osg::WARN) << "shader enable = " <<
	    ((_shadersEnabled) ? "ON" : "OFF") << std::endl;
    for( unsigned int i = 0; i < _programList.size(); i++ )
    {
	//_programList[i]->enable( _shadersEnabled );
    }
}

/*EOF*/
