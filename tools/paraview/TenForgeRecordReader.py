# ParaView reader for TenForge test records (docs/specs/test-record-v1.md).
#
# Load it once in ParaView (Tools > Manage Plugins > Load New, choose this
# file; tick Auto Load to keep it), then File > Open a record's
# manifest.tfrec. The output is the record's analysed curve
# (results/curve.vtp, exports-v1 section 3) with the specimen, procedure
# and conformity as field data. Plot it in a Line Chart View: X array
# strain_percent, Y stress_MPa.
#
# Like TenForge, it reads a record as written or not at all: the curve's
# SHA-256 must match the manifest, or the reader says so and gives nothing.
#
# Pure Python against ParaView's own modules; part of TenForge's tools,
# LGPL-2.1-or-later. No TenForge code is needed to run it.

import hashlib
import json
import os

from paraview.util.vtkAlgorithm import (VTKPythonAlgorithmBase, smdomain, smhint,
                                        smproperty, smproxy)
from vtkmodules.vtkCommonCore import vtkStringArray
from vtkmodules.vtkCommonDataModel import vtkPolyData
from vtkmodules.vtkIOXML import vtkXMLPolyDataReader


def _strings(field_data, name, value):
    a = vtkStringArray()
    a.SetName(name)
    a.InsertNextValue(str(value))
    field_data.AddArray(a)


@smproxy.reader(name="TenForgeRecordReader", label="TenForge test record",
                extensions="tfrec", file_description="TenForge test record")
class TenForgeRecordReader(VTKPythonAlgorithmBase):
    def __init__(self):
        super().__init__(nInputPorts=0, nOutputPorts=1, outputType="vtkPolyData")
        self._filename = None

    @smproperty.stringvector(name="FileName")
    @smdomain.filelist()
    @smhint.filechooser(extensions="tfrec", file_description="TenForge test record")
    def SetFileName(self, name):
        if self._filename != name:
            self._filename = name
            self.Modified()

    def RequestData(self, request, in_info, out_info):
        if not self._filename:
            print("TenForge record: no manifest.tfrec given")
            return 0
        folder = os.path.dirname(os.path.abspath(self._filename))
        with open(self._filename, encoding="utf-8") as f:
            manifest = json.load(f)
        if manifest.get("format") != "tenforge-record":
            print("TenForge record: %s is not a TenForge record manifest" % self._filename)
            return 0
        listed = {e.get("path"): e.get("sha256") for e in manifest.get("files", [])}
        rel = "results/curve.vtp"
        if rel not in listed:
            print("TenForge record: this record has no %s (written by TenForge before 0.3?)" % rel)
            return 0
        path = os.path.join(folder, rel)
        with open(path, "rb") as f:
            digest = hashlib.sha256(f.read()).hexdigest()
        if digest != listed[rel]:
            print("TenForge record: %s has changed since the record was written "
                  "(its SHA-256 no longer matches the manifest)" % rel)
            return 0

        reader = vtkXMLPolyDataReader()
        reader.SetFileName(path)
        reader.Update()
        output = vtkPolyData.GetData(out_info, 0)
        output.ShallowCopy(reader.GetOutput())

        fd = output.GetFieldData()
        test = {}
        if os.path.exists(os.path.join(folder, "test.json")):
            with open(os.path.join(folder, "test.json"), encoding="utf-8") as f:
                test = json.load(f)
        results = {}
        if os.path.exists(os.path.join(folder, "results/results.json")):
            with open(os.path.join(folder, "results/results.json"), encoding="utf-8") as f:
                results = json.load(f)
        specimen = test.get("specimen", {})
        profile = results.get("profile", {})
        _strings(fd, "specimen_id", specimen.get("id", ""))
        _strings(fd, "material", specimen.get("material", ""))
        _strings(fd, "mode", test.get("mode", ""))
        _strings(fd, "procedure", "%s %s" % (profile.get("id", ""), profile.get("profile_version", "")))
        _strings(fd, "conformity", results.get("conformity", {}).get("status", ""))
        _strings(fd, "record", folder)
        return 1
