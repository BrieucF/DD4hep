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

/// Framework include files
#include <DD4hep/DetFactoryHelper.h>
#include <DD4hep/DetectorTools.h>
#include <DD4hep/Printout.h>
#include <XML/Utilities.h>
#include <DDCAD/ASSIMPReader.h>
#include <DDCAD/ASSIMPWriter.h>

// C/C++ include files
#include <filesystem>

using dd4hep::except;
using dd4hep::printout;

/// If the path to the CAD file does not directly exist try to resolve it:
static std::string resolve_path(xml_h e, const std::string& file)   {
  std::error_code errc;
  std::string fname;
  /// Use the xml utilities in the DocumentHandler to resolve the relative path
  if ( file.length() > 7 && file.substr(0,7) == "file://" )
    fname = file.substr(7);
  else
    fname = file;
  if ( !std::filesystem::exists(fname, errc) )   {
    std::string fn = dd4hep::xml::DocumentHandler::system_path(e, fname);
    if ( fn.length() > 7 && fn.substr(0,7) == "file://" )
      fn = fn.substr(7);
    if ( !std::filesystem::exists(fn, errc) )   {
      auto fp = std::filesystem::path(dd4hep::xml::DocumentHandler::system_path(e)).parent_path();
      except("CAD_Shape","+++ CAD file: %s (= %s + %s) is not accessible [%d: %s]",
             fn.c_str(), fp.c_str(), fname.c_str(),
             errc.value(), errc.message().c_str());
    }
    return fn;
  }
  return fname;
}

static void* read_CAD_Volume(dd4hep::Detector& dsc, int argc, char** argv)   {
  std::string fname;
  double scale = 1.0;
  bool   help  = false;
  for(int i = 0; i < argc && argv[i]; ++i)  {
    if (      0 == ::strncmp( "-input",argv[i],4) )  fname = argv[++i];
    else if ( 0 == ::strncmp("--input",argv[i],5) )  fname = argv[++i];
    else if ( 0 == ::strncmp( "-scale",argv[i],4) )  scale = ::atof(argv[++i]);
    else if ( 0 == ::strncmp("--scale",argv[i],5) )  scale = ::atof(argv[++i]);
    else if ( 0 == ::strncmp( "-help",argv[i],2) )   help  = true;
    else if ( 0 == ::strncmp("--help",argv[i],3) )   help  = true;
  }

  if ( fname.empty() || help )    {
    std::cout <<
      "Usage: -plugin DD4hep_CAD_export -arg [-arg]                           \n\n"
      "     -input    <string> Input file name.                                 \n"
      "     -scale    <float>  Scale factor when importing shapes.              \n"
      "     -help              Print this help output.                          \n"
      "     Arguments given: " << dd4hep::arguments(argc,argv) << std::endl << std::flush;
    ::exit(EINVAL);
  }

  auto volumes = dd4hep::cad::ASSIMPReader(dsc).readVolumes(fname, scale);
  if ( volumes.empty() )   {
    except("CAD_Volume","+++ CAD file: %s does not contain any "
           "understandable tessellated volumes.", fname.c_str());
  }
  auto* result = new std::vector<std::unique_ptr<TGeoVolume> >(move(volumes));
  return result;
}
DECLARE_DD4HEP_CONSTRUCTOR(DD4hep_read_CAD_volumes,read_CAD_Volume)

static dd4hep::Handle<TObject> create_CAD_Shape(dd4hep::Detector& dsc, xml_h e)   {
  xml_elt_t elt(e);
  dd4hep::cad::ASSIMPReader rdr(dsc);
  std::string fname = resolve_path(e, elt.attr<std::string>(_U(ref)));
  long        flags = elt.hasAttr(_U(flags)) ? elt.attr<long>(_U(flags))  : 0;
  double      unit  = elt.hasAttr(_U(unit))  ? elt.attr<double>(_U(unit)) : dd4hep::cm;

  if ( flags ) rdr.flags = flags;
  auto shapes = rdr.readShapes(fname, unit);
  if ( shapes.empty() )   {
    except("CAD_Shape","+++ CAD file: %s does not contain any "
           "understandable tessellated shapes.", fname.c_str());
  }
  dd4hep::Solid solid;
  std::size_t count = shapes.size();
  if ( count == 1 )   {
    solid = shapes[0].release();
  }
  else   {
    if ( elt.hasAttr(_U(item)) )  {
      std::size_t which = elt.attr<std::size_t>(_U(item));
      solid = shapes[which].release();
    }
    else if ( elt.hasAttr(_U(mesh)) )  {
      std::size_t which = elt.attr<std::size_t>(_U(mesh));
      solid = shapes[which].release();
    }
    else  {
      except("CAD_Shape","+++ CAD file: %s does contains %ld tessellated shapes. "
             "You need to add a selector.", fname.c_str(), shapes.size());
    }
  }
  if ( elt.hasAttr(_U(name)) ) solid->SetName(elt.attr<std::string>(_U(name)).c_str());
  return solid;
}
DECLARE_XML_SHAPE(CAD_Shape__shape_constructor,create_CAD_Shape)

static dd4hep::Handle<TObject> create_CAD_Assembly(dd4hep::Detector& dsc, xml_h e)   {
  xml_elt_t   elt(e);
  std::string fname = resolve_path(e, elt.attr<std::string>(_U(ref)));
  double      unit  = elt.hasAttr(_U(unit)) ? elt.attr<double>(_U(unit)) : dd4hep::cm;
  auto volumes = dd4hep::cad::ASSIMPReader(dsc).readVolumes(fname, unit);
  if ( volumes.empty() )   {
    except("CAD_Shape","+++ CAD file: %s does not contain any "
           "understandable tessellated volumes.", fname.c_str());
  }
  dd4hep::Assembly assembly("assembly");
  for(std::size_t i=0; i < volumes.size(); ++i)  {
    dd4hep::Volume vol(volumes[i].release());
    assembly.placeVolume(vol);
  }

  if ( elt.hasAttr(_U(name)) ) assembly->SetName(elt.attr<std::string>(_U(name)).c_str());
  return assembly;
}
DECLARE_XML_VOLUME(CAD_Assembly__volume_constructor,create_CAD_Assembly)

/// CAD volume importer plugin
/**
 *
 * The CAD volume plugin allows to embed valumes and shapes originating from
 * Computer Aided Design drawings using multiple formats as they are supported
 * by the open asset importer library (http://assimp.org ).
 * The plugin can be used whenever the xmnl fragment matches the following pattern:
 *
 *   <XXX ref="file-name"  material="material-name">   
 *     <material name="material-name"/>                        <!-- alternative: child or attr -->
 *
 *     Envelope:  Use special envelop shape (default: assembly)
 *                The envelope tag must match the expected pattern of the utility
 *                dd4hep::xml::createStdVolume(Detector& desc, xml::Element e)
 *     <envelope name="volume-name" material="material-name">
 *       <shape name="shape-name" type="shape-type" args....>
 *       </shape>
 *     </envelope>
 *
 *     Option 1:  No additional children. use default material 
 *                and place all children in the origin of the envelope
 *
 *     Option 2:  Volume with default material
 *     <volume name="vol-name"/>
 *
 *     Option 3:  Volume with non-default material
 *     <volume name="vol-name" material="material-name"/>
 *
 *     Option 4:  Volume with optional placement. No position = (0,0,0), No rotation = (0,0,0)
 *     <volume name="vol-name" material="material-name"/>
 *       <position x="0" y="0" z="5*cm"/>
 *       <rotation x="0" y="0" z="0.5*pi*rad"/>
 *     </volume>
 *
 *     For sensitive volumes: add physical volume IDs:
 *     <volume name="vol-name" material="material-name"/>
 *       <physvolid name="layer" value="1"/>
 *       <physvolid name="slice" value="10"/>
 *     </volume>
 *
 *     If flags: (flags>>8)&1 == 1 (257): dump facets
 *
 *   </XXX>
 */
static dd4hep::Handle<TObject> create_CAD_Volume(dd4hep::Detector& dsc, xml_h e)   {
  xml_elt_t   elt(e);
  double      unit  = elt.attr<double>(_U(unit));
  std::string fname = resolve_path(e, elt.attr<std::string>(_U(ref)));
  long        flags = elt.hasAttr(_U(flags)) ? elt.attr<long>(_U(flags))  : 0;
  dd4hep::cad::ASSIMPReader rdr(dsc);

  if ( flags ) rdr.flags = flags;
  auto volumes = rdr.readVolumes(fname, unit);
  if ( volumes.empty() )   {
    except("CAD_Volume","+++ CAD file: %s does not contain any "
           "understandable tessellated volumes.", fname.c_str());
  }
  dd4hep::Volume envelope;
  if ( elt.hasChild(_U(envelope)) )   {
    std::string   typ   = "DD4hep_StdVolume";
    xml_h    x_env = elt.child(_U(envelope));
    TObject* pvol  = dd4hep::PluginService::Create<TObject*>(typ, &dsc, &x_env);
    envelope = dynamic_cast<TGeoVolume*>(pvol);
    if ( !envelope.isValid() )   {
      except("CAD_Volume", "+++ Unable to determine envelope to CAD shape: %s",fname.c_str());
    }
  }
  else   {
    envelope = dd4hep::Assembly("envelope");
  }
  xml_dim_t x_envpos = elt.child(_U(position),false);
  xml_dim_t x_envrot = elt.child(_U(rotation),false);
  dd4hep::Position env_pos;
  dd4hep::RotationZYX env_rot;
  if ( x_envpos && x_envrot )   {
    env_rot = dd4hep::RotationZYX(x_envrot.z(0), x_envrot.y(0), x_envrot.x(0));
    env_pos = dd4hep::Position(x_envpos.x(0), x_envpos.y(0), x_envpos.z(0));
  }
  else if ( x_envpos )
    env_pos = dd4hep::Position(x_envpos.x(0), x_envpos.y(0), x_envpos.z(0));
  else if ( x_envrot )
    env_rot = dd4hep::RotationZYX(x_envrot.z(0), x_envrot.y(0), x_envrot.x(0));

  dd4hep::Transform3D env_trafo(env_rot, env_pos);
  dd4hep::Material    default_material;
  xml_dim_t x_mat = elt.child(_U(material),false);
  if      ( x_mat.ptr() ) default_material = dsc.material(x_mat.nameStr());
  else if ( elt.hasAttr(_U(material)) ) default_material = dsc.material(elt.attr<std::string>(_U(material)));

  if ( elt.hasChild(_U(volume)) )   {
    std::map<int, xml_h> volume_map;
    for (xml_coll_t c(elt,_U(volume)); c; ++c )
      volume_map.emplace(xml_dim_t(c).id(),c);

    for (std::size_t i=0; i < volumes.size(); ++i)   {
      dd4hep::Volume   vol = volumes[i].release();
      dd4hep::Material mat = default_material;
      auto is = volume_map.find(i);
      if ( is == volume_map.end() )   {
        envelope.placeVolume(vol);
      }
      else   {
        xml_dim_t x_vol = (*is).second;
        xml_dim_t x_pos = x_vol.child(_U(position),false);
        xml_dim_t x_rot = x_vol.child(_U(rotation),false);

        if ( x_vol.hasAttr(_U(material)) )  {
          std::string mat_name = x_vol.attr<std::string>(_U(material));
          mat = dsc.material(mat_name);
          if ( !mat.isValid() )
            except("CAD_MultiVolume","+++ Failed to access material "+mat_name);
          vol.setMaterial(mat);
        }
        dd4hep::Position    pos;
        dd4hep::RotationZYX rot;
        if ( x_pos && x_rot )   {
          rot = dd4hep::RotationZYX(x_rot.z(0), x_rot.y(0), x_rot.x(0));
          pos = dd4hep::Position(x_pos.x(0), x_pos.y(0), x_pos.z(0));
        }
        else if ( x_pos )
          pos = dd4hep::Position(x_pos.x(0), x_pos.y(0), x_pos.z(0));
        else if ( x_rot )
          rot = dd4hep::RotationZYX(x_rot.z(0), x_rot.y(0), x_rot.x(0));
      
        dd4hep::PlacedVolume pv = envelope.placeVolume(vol,env_trafo*dd4hep::Transform3D(rot, pos));
        vol.setAttributes(dsc, x_vol.regionStr(), x_vol.limitsStr(), x_vol.visStr());
        for (xml_coll_t cc(x_vol,_U(physvolid)); cc; ++cc )   {
          xml_dim_t vid = cc;
          pv.addPhysVolID(vid.nameStr(), vid.attr<int>(_U(value)));
        }
      }
    }
  }
  else   {
    for(std::size_t i=0; i < volumes.size(); ++i)   {
      dd4hep::Volume vol = volumes[i].release();
      if ( vol.isValid() )   {
        if ( (vol.material() == dsc.air()) && default_material.isValid() )
          vol.setMaterial(default_material);
        envelope.placeVolume(vol,env_trafo);
      }
    }
  }
  if ( elt.hasAttr(_U(name)) ) envelope->SetName(elt.attr<std::string>(_U(name)).c_str());
  return envelope;
}
DECLARE_XML_VOLUME(CAD_MultiVolume__volume_constructor,create_CAD_Volume)

/// CAD volume importer plugin
/**
 *
 */
static long CAD_export(dd4hep::Detector& description, int argc, char** argv)   {
  bool        recursive = false, help = false;
  std::string volume, detector, fname, ftype;
  double      scale = 1.0;
  int         flags = 0;
  
  for(int i = 0; i < argc && argv[i]; ++i)  {
    if (      0 == ::strncmp( "-output",argv[i],4) )    fname     = argv[++i];
    else if ( 0 == ::strncmp("--output",argv[i],5) )    fname     = argv[++i];
    else if ( 0 == ::strncmp( "-type",argv[i],4) )      ftype     = argv[++i];
    else if ( 0 == ::strncmp("--type",argv[i],5) )      ftype     = argv[++i];
    else if ( 0 == ::strncmp( "-detector",argv[i],4) )  detector  = argv[++i];
    else if ( 0 == ::strncmp("--detector",argv[i],5) )  detector  = argv[++i];
    else if ( 0 == ::strncmp( "-volume",argv[i],4) )    volume    = argv[++i];
    else if ( 0 == ::strncmp("--volume",argv[i],5) )    volume    = argv[++i];
    else if ( 0 == ::strncmp( "-recursive",argv[i],4) ) recursive = true;
    else if ( 0 == ::strncmp("--recursive",argv[i],5) ) recursive = true;
    else if ( 0 == ::strncmp( "-scale",argv[i],4) )     scale     = ::atof(argv[++i]);
    else if ( 0 == ::strncmp("--scale",argv[i],5) )     scale     = ::atof(argv[++i]);
    else if ( 0 == ::strncmp( "-flags",argv[i],4) )     flags     = ::atol(argv[++i]);
    else if ( 0 == ::strncmp("--flags",argv[i],5) )     flags     = ::atol(argv[++i]);
    else if ( 0 == ::strncmp( "-help",argv[i],2) )      help      = true;
    else if ( 0 == ::strncmp("--help",argv[i],3) )      help      = true;
  }

  if ( fname.empty() || ftype.empty() ) help = true;
  if ( volume.empty() && detector.empty() ) help = true;
  if ( help )   {
    std::cout <<
      "Usage: -plugin DD4hep_CAD_export -arg [-arg]                           \n\n"
      "     -output   <string> Output file name.                                \n"
      "     -type     <string> Output file type.                                \n"
      "     -recursive         Export volume/detector element and all daughters.\n"
      "     -volume   <string> Path to the volume to be exported.               \n"
      "     -detector <string> Path to the detector element to be exported.     \n"
      "     -help              Print this help output.                          \n"
      "     -scale    <number> Unit scale before writing output data.           \n"
      "     -flags    <number> Flagsging helper to pass args -- Experts only.   \n"
      "     Arguments given: " << dd4hep::arguments(argc,argv) << std::endl << std::flush;
    ::exit(EINVAL);
  }

  dd4hep::PlacedVolume pv;
  if ( !detector.empty() )   {
    dd4hep::DetElement elt;
    if ( detector == "/world" )
      elt = description.world();
    else
      elt = dd4hep::detail::tools::findElement(description,detector);
    if ( !elt.isValid() )  {
      except("DD4hep_CAD_export","+++ Invalid DetElement path: %s",detector.c_str());
    }
    if ( !elt.placement().isValid() )   {
      except("DD4hep_CAD_export","+++ Invalid DetElement placement: %s",detector.c_str());
    }
    pv = elt.placement();
  }
  else if ( !volume.empty() )   {
    pv = dd4hep::detail::tools::findNode(description.world().placement(), volume);
    if ( !pv.isValid() )   {
      except("DD4hep_CAD_export","+++ Invalid placement path: %s",volume.c_str());
    }
  }
  dd4hep::cad::ASSIMPWriter wr(description);
  if ( flags ) wr.flags = flags;
  std::vector<dd4hep::PlacedVolume> places {pv};
  auto num_mesh = wr.write(fname, ftype, places, recursive, scale);
  if ( num_mesh < 0 )   {
    printout(dd4hep::ERROR, "DD4hep_CAD_export",
             "+++ Failed to export shapes to CAD file: %s [%s]",
             fname.c_str(), ftype.c_str());
  }
  return 1;
}
DECLARE_APPLY(DD4hep_CAD_export,CAD_export)
