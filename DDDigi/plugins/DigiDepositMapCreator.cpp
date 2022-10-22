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

// Framework include files
#include <DDDigi/DigiContext.h>
#include <DDDigi/DigiContainerProcessor.h>

/// Namespace for the AIDA detector description toolkit
namespace dd4hep {

  /// Namespace for the Digitization part of the AIDA detector description toolkit
  namespace digi {

    /// Actor to select energy deposits according to the supplied segmentation
    /** Actor to select energy deposits according to the supplied segmentation
     *
     *  The selected deposits are placed in the output container
     *  supplied by the arguments.
     *
     *  \author  M.Frank
     *  \version 1.0
     *  \ingroup DD4HEP_DIGITIZATION
     */
    class DigiDepositMapCreator : public DigiContainerProcessor   {
    public:
      /// Standard constructor
      using DigiContainerProcessor::DigiContainerProcessor;

      template <typename T> void create_deposits(const T& cont, work_t& work)  const  {
	Key key(cont.name, work.output.mask);
	DepositMapping* mapping = work.output.data.pointer<DepositMapping>(key);
	if ( !mapping )   {
	  DepositMapping m(cont.name, work.output.mask);
	  work.output.data.put(m.key, std::move(m));
	  mapping = work.output.data.pointer<DepositMapping>(key);
	}
	std::size_t start = mapping->size();
	for( const auto& dep : cont )
	  mapping->data.emplace(dep.first, EnergyDeposit());
	std::size_t end   = mapping->size();
	info("+++ %-32s added %6ld entries (now: %6ld) from mask: %04X to mask: %04X",
	     cont.name.c_str(), end-start, end, cont.key.mask(), mapping->key.mask());
      }
      /// Main functional callback
      virtual void execute(DigiContext&, work_t& work)  const override final  {
	if ( const auto* m = work.get_input<DepositMapping>() )
	  create_deposits(*m, work);
	else if ( const auto* v = work.get_input<DepositVector>() )
	  create_deposits(*v, work);
	else
	  except("Request to handle unknown data type: %s", work.input_type_name().c_str());
      }
    };
  }    // End namespace digi
}      // End namespace dd4hep

#include <DDDigi/DigiFactories.h>
DECLARE_DIGIACTION_NS(dd4hep::digi,DigiDepositMapCreator)
