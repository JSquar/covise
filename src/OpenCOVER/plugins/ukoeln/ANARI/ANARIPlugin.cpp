/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */
#include "ANARIPlugin.h"

#include <cover/ui/Button.h>
#include <cover/ui/ButtonGroup.h>
#include <cover/ui/Menu.h>
#include <cover/ui/Slider.h>
#include <cover/coVRPluginSupport.h>
#include <cover/RenderObject.h>
#include <config/CoviseConfig.h>

/* extern definitions for type constants */
static int TYPE_HEXAGON = 7;
static int TYPE_HEXAEDER = 7;
static int TYPE_PRISM = 6;
static int TYPE_PYRAMID = 5;
static int TYPE_TETRAHEDER = 4;
static int TYPE_QUAD = 3;
static int TYPE_TRIANGLE = 2;
static int TYPE_BAR = 1;
static int TYPE_NONE = 0;
static int TYPE_POINT = 10;

static uint8_t ANARI_TETRAHEDRON = 10;
static uint8_t ANARI_HEXAHEDRON = 12;
static uint8_t ANARI_WEDGE = 13;
static uint8_t ANARI_PYRAMID = 14;

ANARIPlugin *ANARIPlugin::plugin = nullptr;

static FileHandler handlers[] = {
    { NULL,
      ANARIPlugin::loadMesh,
      ANARIPlugin::unloadMesh,
      "obj" },
    { NULL,
      ANARIPlugin::loadMesh,
      ANARIPlugin::unloadMesh,
      "gltf" },
    { NULL,
      ANARIPlugin::loadMesh,
      ANARIPlugin::unloadMesh,
      "glb" },
    { NULL,
      ANARIPlugin::loadMesh,
      ANARIPlugin::unloadMesh,
      "ply" },
    { NULL,
      ANARIPlugin::loadVolumeRAW,
      ANARIPlugin::unloadVolumeRAW,
      "raw" },
    { NULL,
      ANARIPlugin::loadFLASH,
      ANARIPlugin::unloadFLASH,
      "hdf5" },
    { NULL,
      ANARIPlugin::loadUMeshVTK,
      ANARIPlugin::unloadUMeshVTK,
      "vtk" },
    { NULL,
      ANARIPlugin::loadUMeshVTK,
      ANARIPlugin::unloadUMeshVTK,
      "vtu" },
};

ANARIPlugin *ANARIPlugin::instance()
{
    return plugin;
}

int ANARIPlugin::loadMesh(const char *fileName, osg::Group *loadParent, const char *)
{
    if (plugin->renderer)
        plugin->renderer->loadMesh(fileName);

    return 1;
}

int ANARIPlugin::unloadMesh(const char *fileName, const char *)
{
    if (plugin->renderer)
        plugin->renderer->unloadMesh(fileName);

    return 1;
}

int ANARIPlugin::loadVolumeRAW(const char *fileName, osg::Group *loadParent, const char *)
{
    if (plugin->renderer)
        plugin->renderer->loadVolumeRAW(fileName);

    return 1;
}

int ANARIPlugin::unloadVolumeRAW(const char *fileName, const char *)
{
    if (plugin->renderer)
        plugin->renderer->unloadVolumeRAW(fileName);

    return 1;
}

int ANARIPlugin::loadFLASH(const char *fileName, osg::Group *loadParent, const char *)
{
    if (plugin->renderer)
        plugin->renderer->loadFLASH(fileName);

    return 1;
}

int ANARIPlugin::unloadFLASH(const char *fileName, const char *)
{
    if (plugin->renderer)
        plugin->renderer->unloadFLASH(fileName);

    return 1;
}

int ANARIPlugin::loadUMeshVTK(const char *fileName, osg::Group *loadParent, const char *)
{
    if (plugin->renderer)
        plugin->renderer->loadUMeshVTK(fileName);

    return 1;
}

int ANARIPlugin::unloadUMeshVTK(const char *fileName, const char *)
{
    if (plugin->renderer)
        plugin->renderer->unloadUMeshVTK(fileName);

    return 1;
}

ANARIPlugin::ANARIPlugin()
: coVRPlugin(COVER_PLUGIN_NAME)
, ui::Owner("ANARI",cover->ui)
{
}

ANARIPlugin::~ANARIPlugin()
{
    int numHandlers = sizeof(handlers) / sizeof(handlers[0]);
    for (int i=0; i<numHandlers; ++i) {
        coVRFileManager::instance()->unregisterFileHandler(&handlers[i]);
    }
}

bool ANARIPlugin::init()
{
    if (cover->debugLevel(1)) fprintf(stderr, "\n    ANARIPlugin::init\n");

    if (plugin) return false;

    plugin = this;

    renderer = std::make_shared<Renderer>();

    // register file handlers
    int numHandlers = sizeof(handlers) / sizeof(handlers[0]);
    for (int i=0; i<numHandlers; ++i) {
        coVRFileManager::instance()->registerFileHandler(&handlers[i]);
    }

    // init menu
    buildUI();

    return true;
}

void ANARIPlugin::preFrame()
{
    if (!renderer)
        return;

    renderer->renderFrame();
}

void ANARIPlugin::expandBoundingSphere(osg::BoundingSphere &bs)
{
    if (!renderer)
        return;

    renderer->expandBoundingSphere(bs);
}

void ANARIPlugin::addObject(const RenderObject *container, osg::Group *parent,
                            const RenderObject *geometry, const RenderObject *normals,
                            const RenderObject *colors, const RenderObject *texture)
{
    (void)container;
    (void)parent;
    (void)normals;
    (void)texture;

    if (geometry->isUniformGrid()) {
        int sizeX, sizeY, sizeZ;
        geometry->getSize(sizeX, sizeY, sizeZ);
        const uint8_t *byteData = colors->getByte(Field::Byte);

        if (sizeX && sizeY && sizeZ && byteData) {
            renderer->loadVolume(byteData, sizeX, sizeY, sizeZ, 1,
                                 colors->getMin(0), colors->getMax(0));
        }
    } else if (geometry->isUnstructuredGrid() && colors) {
        int numCells, numIndices, numVerts;
        geometry->getSize(numCells, numIndices, numVerts);

        // Prefix into index array
        auto *cellIndex = geometry->getInt(Field::Elements);

        // Element indices
        auto *index = geometry->getInt(Field::Connections);

        // Element types
        auto *type = geometry->getInt(Field::Types);

        auto *X = geometry->getFloat(Field::X);
        auto *Y = geometry->getFloat(Field::Y);
        auto *Z = geometry->getFloat(Field::Z);

        auto *vertexData = colors->getFloat(Field::Red);

        if (cellIndex && index && X && Y && Z && vertexData) {
            // TODO: ANARI device(s) should support int32 indices!
            std::vector<uint64_t> cellIndex64(numCells);
            for (size_t i=0; i<numCells; ++i) {
                cellIndex64[i] = cellIndex[i];
            }

            // TODO: dito
            std::vector<uint64_t> index64(numIndices);
            for (size_t i=0; i<numIndices; ++i) {
                index64[i] = index[i];
            }

            // Some devices (e.g., ospray) require types
            std::vector<uint8_t> type8(numCells);
            for (size_t i=0; i<numCells; ++i) {
                if (type[i] == TYPE_TETRAHEDER)
                    type8[i] = ANARI_TETRAHEDRON;
                else if (type[i] == TYPE_PYRAMID)
                    type8[i] = ANARI_PYRAMID;
                else if (type[i] == TYPE_PRISM)
                    type8[i] = ANARI_WEDGE;
                else if (type[i] == TYPE_HEXAEDER)
                    type8[i] = ANARI_HEXAHEDRON;
                else
                    printf("Error %i is an unsupported cell type\n", type[i]);
            }

            float minValue = HUGE_VAL, maxValue = -HUGE_VAL;
            std::vector<float> vertexPosition(numVerts*3);
            for (size_t i=0; i<numVerts; ++i) {
                vertexPosition[i*3]   = X[i];
                vertexPosition[i*3+1] = Y[i];
                vertexPosition[i*3+2] = Z[i];

                minValue = std::min(minValue, vertexData[i]);
                maxValue = std::max(maxValue, vertexData[i]);
            }

            renderer->loadUMesh(vertexPosition.data(), cellIndex64.data(), index64.data(),
                                type8.data(), vertexData, numCells, numIndices, numVerts,
                                minValue, maxValue);
        }
    }
}

void ANARIPlugin::removeObject(const char *objName, bool replaceFlag)
{
    // NO!
}

void ANARIPlugin::buildUI()
{
    if (!anariMenu)
        anariMenu = new ui::Menu("ANARI", this);

    if (!rendererMenu)
        rendererMenu = new ui::Menu(anariMenu, "Renderer");

    if (!rendererGroup)
        rendererGroup = new ui::Group(rendererMenu, "Renderer");

    if (!rendererButtonGroup)
        rendererButtonGroup = new ui::ButtonGroup(rendererGroup, "RendererGroup");

    std::vector<std::string> rendererTypes = renderer->getRendererTypes();
    if (rendererTypes.size() != rendererButtons.size()) {
        rendererButtons.resize(rendererTypes.size());

        for (size_t i=0; i<rendererTypes.size(); ++i) {
            rendererButtons[i] = new ui::Button(rendererGroup,
                                                rendererTypes[i],
                                                rendererButtonGroup);
            rendererButtons[i]->setText(rendererTypes[i]);
            rendererButtons[i]->setCallback([=](bool state) {
                if (state) {
                    renderer->setRendererType(rendererTypes[i]);
                    this->previousRendererType = this->rendererType;
                    this->rendererType = i;
                    if (this->previousRendererType != this->rendererType)
                        buildUI();
                }
            });
        }
    }

    if (this->previousRendererType != this->rendererType) {
        tearDownUI();
        auto &rendererParameters = renderer->getRendererParameters();
        for (auto &p : rendererParameters[this->rendererType]) {
            rendererUI.push_back(ui_anari::buildUI(renderer, p, anariMenu));
        }
    }
}

void ANARIPlugin::tearDownUI()
{
    for (size_t i=0; i<rendererUI.size(); ++i) {
        auto *elem = rendererUI[i];
        if (elem != nullptr) {
            anariMenu->remove(elem);
            delete elem;
        }
    }
    rendererUI.clear();
}

COVERPLUGIN(ANARIPlugin)
