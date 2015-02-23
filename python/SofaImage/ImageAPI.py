import os.path

import SofaPython.Tools
import SofaPython.units

class Image:

    class Mesh:
        # TODO add support for ROI
        def __init__(self):
            self.mesh=None
            self.visual=None
            self.value=None
            self.closingValue=None

    def __init__(self, parentNode, name, imageType="ImageUC"):
        self.imageType = imageType
        self.node = parentNode.createChild("image_"+name)
        self.name = name
        self.meshes = dict()
        self.value = None
        self.closingValue = None
        self.image = None
        self.viewer = None
        self.exporter = None

    def addMeshLoader(self, meshFile, value, closingValue=None, name=None):
        mesh = Image.Mesh()
        mesh.value = value
        mesh.closingValue = value if closingValue is None else closingValue
        _name = name if not name is None else os.path.splitext(os.path.basename(meshFile))[0]
        mesh.mesh = SofaPython.Tools.meshLoader(self.node, meshFile, name="meshLoader_"+_name)
        self.meshes[_name] = mesh

    def addMeshVisual(self, meshName):
        if self.meshes[meshName].mesh is None:
            print "[ImageAPI.Image] ERROR: no mesh for", meshName
        self.meshes[meshName].visual = self.node.createObject("VisualModel", name="visual_"+meshName, src="@"+SofaPython.Tools.getObjectPath(self.meshes[meshName].mesh))

    def addMeshToImage(self, voxelSize):
        args=dict()
        i=1
        for name,mesh in self.meshes.iteritems():
            if mesh.mesh is None:
                print "[ImageAPI.Image] ERROR: no mesh for", name
            meshPath = SofaPython.Tools.getObjectPath(mesh.mesh)
            args["position"+(str(i) if i>1 else "")]="@"+meshPath+".position"
            args["triangles"+(str(i) if i>1 else "")]="@"+meshPath+".triangles"
            args["value"+(str(i) if i>1 else "")]=mesh.value
#            args["closingValue"+(str(i) if i>1 else "")]=mesh.closingValue not yet a closingValue per mesh !
            args["closingValue"]=mesh.closingValue # last wins
            i+=1
        self.image = self.node.createObject('MeshToImageEngine', template=self.imageType, name="image", voxelSize=SofaPython.units.length_from_SI(voxelSize), subdiv=8, fillInside="true", rotateImage="false", nbMeshes=len(self.meshes), **args)

    def addViewer(self):
        if self.image is None:
            print "[ImageAPI.Image] ERROR: no image"
        imagePath = SofaPython.Tools.getObjectPath(self.image)
        self.viewer = self.node.createObject('ImageViewer', name="viewer", template=self.imageType, image="@"+imagePath+".image", transform="@"+imagePath+".transform")

    def addContainer(self, filename):
        self.image = self.node.createObject('ImageContainer', template=self.imageType, name="image", filename=filename)

    def addExporter(self, filename):
        if self.image is None:
            print "[ImageAPI.Image] ERROR: no image"
        imagePath = SofaPython.Tools.getObjectPath(self.image)
        self.exporter = self.node.createObject('ImageExporter', template=self.imageType, name="exporter", image="@"+imagePath+".image", transform="@"+imagePath+".transform", filename=filename, exportAtEnd=True, printLog=True)
