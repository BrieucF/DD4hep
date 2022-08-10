//==========================================================================
//  AIDA Detector description implementation 
//--------------------------------------------------------------------------
// Copyright (C) Organisation europeenne pour la Recherche nucleaire (CERN)
// All rights reserved.
//
// For the licensing terms see $DD4hepINSTALL/LICENSE.
// For the list of contributors see $DD4hepINSTALL/doc/CREDITS.
//
// Author     : M.Frank
//
//==========================================================================

// Framework includes
#include <DD4hep/DetFactoryHelper.h>
#include <XML/Utilities.h>

using namespace dd4hep;
using namespace dd4hep::detail;

static Ref_t create_detector(Detector& description, xml_h e, SensitiveDetector sens)  {
  xml_dim_t x_det = e;  
  xml_comp_t xshape (x_det.child(_U(shape)));
  xml_dim_t  xpos   (x_det.child(_U(position), false));
  xml_elt_t  xmat   (x_det.child(_U(material)));
  xml_dim_t  xbox   (x_det.child(_U(box)));
  DetElement d_det(x_det.nameStr(),x_det.id());
  Volume assembly(x_det.nameStr()+"_vol", Box(xbox.x(), xbox.y(), xbox.z()), description.air());
  //Assembly  assembly(x_det.nameStr()+"_envelope");
  Material   mat    (description.material(xmat.attr<std::string>(_U(name))));
  Position   pos(xpos.x(),xpos.y(),xpos.z());
  Solid  sh  = xml::createShape(description, xshape.typeStr(), xshape);
  Volume vol = Volume(x_det.nameStr()+"_vol", sh, mat);
  PlacedVolume pv;

  for(xml_coll_t c(xshape,_U(property)); c; ++c)   {
    xml_elt_t ec = c;
    vol.addProperty(ec.attr<std::string>(_U(name)), ec.attr<std::string>(_U(value)));
  }

  sens.setType("calorimeter");
  vol.setVisAttributes(description, x_det.visStr());
  vol.setSensitiveDetector(sens);

  int ipos = 0;
  for(xml_coll_t c(xshape, _U(position)); c; ++c)   {
    xml_dim_t  xp = c;
    Position   vol_pos(xp.x(), xp.y(), xp.z());
    pv = assembly.placeVolume(vol, vol_pos);
    pv.addPhysVolID("module", ++ipos);
  }

  assembly.setVisAttributes(description, xbox.attr<std::string>(_U(vis)));
  pv = description.pickMotherVolume(d_det).placeVolume(assembly, pos);
  pv.addPhysVolID("system",x_det.id());
  d_det.setPlacement(pv);
  return d_det;
}

DECLARE_DETELEMENT(DD4hep_SingleShape,create_detector)
