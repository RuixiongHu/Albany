/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
*                                                                    *
* Notice: This computer software was prepared by Sandia Corporation, *
* hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
* the Department of Energy (DOE). All rights in the computer software*
* are reserved by DOE on behalf of the United States Government and  *
* the Contractor as provided in the Contract. You are authorized to  *
* use this computer software for Governmental purposes but it is not *
* to be released or distributed to the public. NEITHER THE GOVERNMENT*
* NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
* ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Andy Salinger, agsalin@sandia.gov                  *
\********************************************************************/


#include <fstream>
#include "Teuchos_TestForException.hpp"
#include "Teuchos_CommHelpers.hpp"

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************

template<typename Traits>
void 
QCAD::FieldValueScatterScalarResponse<PHAL::AlbanyTraits::Jacobian, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Here we scatter the *global* response
  Teuchos::RCP<Epetra_Vector> g = workset.g;
  if (g != Teuchos::null)
    for (std::size_t res = 0; res < this->field_components.size(); res++) {
      (*g)[res] = this->global_response[this->field_components[res]].val();
  }

  Teuchos::RCP<Epetra_MultiVector> dgdx = workset.dgdx;
  Teuchos::RCP<Epetra_MultiVector> dgdxdot = workset.dgdxdot;
  Teuchos::RCP<Epetra_MultiVector> overlapped_dgdx = workset.overlapped_dgdx;
  Teuchos::RCP<Epetra_MultiVector> overlapped_dgdxdot = 
    workset.overlapped_dgdxdot;
  Teuchos::RCP<Epetra_MultiVector> dg, overlapped_dg;
  if (dgdx != Teuchos::null) {
    dg = dgdx;
    overlapped_dg = overlapped_dgdx;
  }
  else {
    dg = dgdxdot;
    overlapped_dg = overlapped_dgdxdot;
  }

  dg->PutScalar(0.0);
  overlapped_dg->PutScalar(0.0);
   
  // Extract derivatives for the cell corresponding to nodeID
  if (nodeID != Teuchos::null) {

    // Loop over responses
    for (std::size_t res = 0; res < this->field_components.size(); res++) {
      ScalarT& val = this->global_response(this->field_components[res]);
      
      // Loop over nodes in cell
      for (unsigned int node_dof=0; node_dof<numNodes; node_dof++) {
	int neq = nodeID[node_dof].size();
	
	// Loop over equations per node
	for (unsigned int eq_dof=0; eq_dof<neq; eq_dof++) {
	  
	  // local derivative component
	  int deriv = neq * node_dof + eq_dof;
	  
	  // local DOF
	  int dof = nodeID[node_dof][eq_dof];
	  
	  // Set dg/dx
	  overlapped_dg->ReplaceMyValue(dof, res, val.dx(deriv));
	  
	} // column equations
      } // column nodes
    } // response
  } // cell belongs to this proc

  dg->Export(*overlapped_dg, *workset.x_importer, Insert);
}

// **********************************************************************
// Specialization: Stochastic Galerkin Jacobian
// **********************************************************************

template<typename Traits>
void 
QCAD::FieldValueScatterScalarResponse<PHAL::AlbanyTraits::SGJacobian, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Here we scatter the *global* SG response
  Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > g_sg = workset.sg_g;
  if (g_sg != Teuchos::null) {
    for (std::size_t res = 0; res < this->field_components.size(); res++) {
      ScalarT& val = this->global_response[this->field_components[res]];
      for (int block=0; block<g_sg->size(); block++)
	(*g_sg)[block][res] = val.val().coeff(block);
    }
  }

  // Here we scatter the *global* SG response derivatives
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dgdx_sg = 
    workset.sg_dgdx;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dgdxdot_sg = 
    workset.sg_dgdxdot;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> overlapped_dgdx_sg = 
    workset.overlapped_sg_dgdx;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> overlapped_dgdxdot_sg = 
    workset.overlapped_sg_dgdxdot;

  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dg_sg, overlapped_dg_sg;
  if (dgdx_sg != Teuchos::null) {
    dg_sg = dgdx_sg;
    overlapped_dg_sg = overlapped_dgdx_sg;
  }
  else {
    dg_sg = dgdxdot_sg;
    overlapped_dg_sg = overlapped_dgdxdot_sg;
  }

  dg_sg->init(0.0);
  overlapped_dg_sg->init(0.0);

  // Extract derivatives for the cell corresponding to nodeID
  if (nodeID != Teuchos::null) {

    // Loop over responses
    for (std::size_t res = 0; res < this->field_components.size(); res++) {
      ScalarT& val = this->global_response(this->field_components[res]);
      
      // Loop over nodes in cell
      for (unsigned int node_dof=0; node_dof<numNodes; node_dof++) {
	int neq = nodeID[node_dof].size();
	
	// Loop over equations per node
	for (unsigned int eq_dof=0; eq_dof<neq; eq_dof++) {
	  
	  // local derivative component
	  int deriv = neq * node_dof + eq_dof;
	  
	  // local DOF
	  int dof = nodeID[node_dof][eq_dof];
	  
	  // Set dg/dx
	  for (int block=0; block<dg_sg->size(); block++)
	    (*overlapped_dg_sg)[block].ReplaceMyValue(dof, res, 
						      val.dx(deriv).coeff(block));
	  
	} // column equations
      } // response
    } // node
  } // cell belongs to this proc

  for (int block=0; block<dgdx_sg->size(); block++)
    (*dg_sg)[block].Export((*overlapped_dg_sg)[block], 
			   *workset.x_importer, Insert);
}

// **********************************************************************
// Specialization: Multi-point Jacobian
// **********************************************************************

template<typename Traits>
void 
QCAD::FieldValueScatterScalarResponse<PHAL::AlbanyTraits::MPJacobian, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Here we scatter the *global* MP response
  Teuchos::RCP<Stokhos::ProductEpetraVector> g_mp = workset.mp_g;
  if (g_mp != Teuchos::null) {
    for (std::size_t res = 0; res < this->field_components.size(); res++) {
      ScalarT& val = this->global_response[this->field_components[res]];
      for (int block=0; block<g_mp->size(); block++)
	(*g_mp)[block][res] = val.val().coeff(block);
    }
  }

  // Here we scatter the *global* MP response derivatives
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dgdx_mp = 
    workset.mp_dgdx;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dgdxdot_mp = 
    workset.mp_dgdxdot;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> overlapped_dgdx_mp = 
    workset.overlapped_mp_dgdx;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> overlapped_dgdxdot_mp = 
    workset.overlapped_mp_dgdxdot;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dg_mp, overlapped_dg_mp;
  if (dgdx_mp != Teuchos::null) {
    dg_mp = dgdx_mp;
    overlapped_dg_mp = overlapped_dgdx_mp;
  }
  else {
    dg_mp = dgdxdot_mp;
    overlapped_dg_mp = overlapped_dgdxdot_mp;
  }

  dg_mp->init(0.0);
  overlapped_dg_mp->init(0.0);

  // Extract derivatives for the cell corresponding to nodeID
  if (nodeID != Teuchos::null) {

    // Loop over responses
    for (std::size_t res = 0; res < this->field_components.size(); res++) {
      ScalarT& val = this->global_response(this->field_components[res]);
      
      // Loop over nodes in cell
      for (unsigned int node_dof=0; node_dof<numNodes; node_dof++) {
	int neq = nodeID[node_dof].size();
	
	// Loop over equations per node
	for (unsigned int eq_dof=0; eq_dof<neq; eq_dof++) {
	  
	  // local derivative component
	  int deriv = neq * node_dof + eq_dof;
	  
	  // local DOF
	  int dof = nodeID[node_dof][eq_dof];
	  
	  // Set dg/dx
	  for (int block=0; block<dg_mp->size(); block++)
	    (*overlapped_dg_mp)[block].ReplaceMyValue(dof, res, 
						      val.dx(deriv).coeff(block));
	  
	} // column equations
      } // response
    } // node
  } // cell belongs to this proc

  for (int block=0; block<dgdx_mp->size(); block++)
    (*dg_mp)[block].Export((*overlapped_dg_mp)[block], 
			   *workset.x_importer, Insert);
}

template<typename EvalT, typename Traits>
QCAD::ResponseFieldValue<EvalT, Traits>::
ResponseFieldValue(Teuchos::ParameterList& p,
		   const Teuchos::RCP<Albany::Layouts>& dl) :
  coordVec("Coord Vec", dl->qp_vector),
  weights("Weights", dl->qp_scalar)
{
  // get and validate Response parameter list
  Teuchos::ParameterList* plist = 
    p.get<Teuchos::ParameterList*>("Parameter List");
  Teuchos::RCP<const Teuchos::ParameterList> reflist = 
    this->getValidResponseParameters();
  plist->validateParameters(*reflist,0);

  // number of quad points per cell and dimension of space
  Teuchos::RCP<PHX::DataLayout> scalar_dl = dl->qp_scalar;
  Teuchos::RCP<PHX::DataLayout> vector_dl = dl->qp_vector;
  
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  // User-specified parameters
  operation    = plist->get<std::string>("Operation");

  bOpFieldIsVector = false;
  if(plist->isParameter("Operation Vector Field Name")) {
    opFieldName  = plist->get<std::string>("Operation Vector Field Name");
    bOpFieldIsVector = true;
  }
  else opFieldName  = plist->get<std::string>("Operation Field Name");
 
  bRetFieldIsVector = false;
  if(plist->isParameter("Return Vector Field Name")) {
    retFieldName  = plist->get<std::string>("Return Vector Field Name");
    bRetFieldIsVector = true;
  }
  else retFieldName = plist->get<std::string>("Return Field Name", opFieldName);
  bReturnOpField = (opFieldName == retFieldName);

  opDomain     = plist->get<std::string>("Operation Domain", "box");
  opX = plist->get<bool>("Operate on x-component", true) && (numDims > 0);
  opY = plist->get<bool>("Operate on y-component", true) && (numDims > 1);
  opZ = plist->get<bool>("Operate on z-component", true) && (numDims > 2);

  if(opDomain == "box") {
    limitX = limitY = limitZ = false;

    if( plist->isParameter("x min") && plist->isParameter("x max") ) {
      limitX = true; TEUCHOS_TEST_FOR_EXCEPT(numDims <= 0);
      xmin = plist->get<double>("x min");
      xmax = plist->get<double>("x max");
    }
    if( plist->isParameter("y min") && plist->isParameter("y max") ) {
      limitY = true; TEUCHOS_TEST_FOR_EXCEPT(numDims <= 1);
      ymin = plist->get<double>("y min");
      ymax = plist->get<double>("y max");
    }
    if( plist->isParameter("z min") && plist->isParameter("z max") ) {
      limitZ = true; TEUCHOS_TEST_FOR_EXCEPT(numDims <= 2);
      zmin = plist->get<double>("z min");
      zmax = plist->get<double>("z max");
    }
  }
  else if(opDomain == "element block") {
    ebName = plist->get<string>("Element Block Name");
  }
  else TEUCHOS_TEST_FOR_EXCEPTION (true, Teuchos::Exceptions::InvalidParameter, std::endl 
             << "Error!  Invalid operation domain type " << opDomain << std::endl); 


  // setup operation field and return field (if it's a different field)
  if(bOpFieldIsVector) {
    PHX::MDField<ScalarT> f(opFieldName, vector_dl); opField = f; }
  else {
    PHX::MDField<ScalarT> f(opFieldName, scalar_dl); opField = f; }

  if(!bReturnOpField) {
    if(bRetFieldIsVector) {
      PHX::MDField<ScalarT> f(retFieldName, vector_dl); retField = f; }
    else {
      PHX::MDField<ScalarT> f(retFieldName, scalar_dl); retField = f; }
  }

  // add dependent fields
  this->addDependentField(opField);
  this->addDependentField(coordVec);
  this->addDependentField(weights);
  if(!bReturnOpField) this->addDependentField(retField); //when return field is *different* from op field

  // Set sentinal values for max/min problems 
  initVals = Teuchos::Array<double>(5, 0.0);
  if( operation == "Maximize" ) initVals[1] = -1e200;
  else if( operation == "Minimize" ) initVals[1] = 1e100;
  else TEUCHOS_TEST_FOR_EXCEPTION (
    true, Teuchos::Exceptions::InvalidParameter, std::endl 
    << "Error!  Invalid operation type " << operation << std::endl); 

  this->setName(opFieldName+" Response Field Value"+PHX::TypeString<EvalT>::value);

  // Setup scatter evaluator
  std::string global_response_name = 
    opFieldName + " Global Response Field Value";
  int worksetSize = scalar_dl->dimension(0);
  int responseSize = 5;
  Teuchos::RCP<PHX::DataLayout> global_response_layout =
    Teuchos::rcp(new PHX::MDALayout<Dim>(responseSize));
  PHX::Tag<ScalarT> global_response_tag(global_response_name, 
					global_response_layout);
  p.set("Stand-alone Evaluator", false);
  p.set("Global Response Field Tag", global_response_tag);
  this->setup(p,dl);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseFieldValue<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(opField,fm);
  this->utils.setFieldData(coordVec,fm);
  this->utils.setFieldData(weights,fm);
  if(!bReturnOpField) this->utils.setFieldData(retField,fm);
  QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::postRegistrationSetup(d,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseFieldValue<EvalT, Traits>::
preEvaluate(typename Traits::PreEvalData workset)
{
  for (typename PHX::MDField<ScalarT>::size_type i=0; 
       i<this->global_response.size(); i++)
    this->global_response[i] = initVals[i];

  // Do global initialization
  QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::preEvaluate(workset);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseFieldValue<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  ScalarT opVal, qpVal, cellVol;

  if(opDomain == "element block" && workset.EBName != ebName) 
  {
      return;
  }

  for (std::size_t cell=0; cell < workset.numCells; ++cell) 
  {
    // If operation domain is a "box", check whether the current cell is 
    //  at least partially contained within the box
    if(opDomain == "box") {
      bool cellInBox = false;
      for (std::size_t qp=0; qp < numQPs; ++qp) {
        if( (!limitX || (coordVec(cell,qp,0) >= xmin && coordVec(cell,qp,0) <= xmax)) &&
            (!limitY || (coordVec(cell,qp,1) >= ymin && coordVec(cell,qp,1) <= ymax)) &&
            (!limitZ || (coordVec(cell,qp,2) >= zmin && coordVec(cell,qp,2) <= zmax)) ) {
          cellInBox = true; break; }
      }
      if( !cellInBox ) continue;
    }

    // Get the cell volume, used for averaging over a cell
    cellVol = 0.0;
    for (std::size_t qp=0; qp < numQPs; ++qp)
      cellVol += weights(cell,qp);

    // Get the scalar value of the field being operated on which will be used
    //  in the operation (all operations just deal with scalar data so far)
    opVal = 0.0;
    for (std::size_t qp=0; qp < numQPs; ++qp) {
      qpVal = 0.0;
      if(bOpFieldIsVector) {
        if(opX) qpVal += opField(cell,qp,0) * opField(cell,qp,0);
        if(opY) qpVal += opField(cell,qp,1) * opField(cell,qp,1);
        if(opZ) qpVal += opField(cell,qp,2) * opField(cell,qp,2);
      }
      else qpVal = opField(cell,qp);
      opVal += qpVal * weights(cell,qp);
    }
    opVal /= cellVol;  
    // opVal = the average value of the field operated on over the current cell

      
    // Check if the currently stored min/max value needs to be updated
    if( (operation == "Maximize" && opVal > this->global_response[1]) ||
        (operation == "Minimize" && opVal < this->global_response[1]) ) {
      max_nodeID = workset.wsElNodeEqID[cell];

      // set g[0] = value of return field at the current cell (avg)
      this->global_response[0]=0.0;
      if(bReturnOpField) {
        for (std::size_t qp=0; qp < numQPs; ++qp) {
          qpVal = 0.0;
          if(bOpFieldIsVector) {
            for(std::size_t i=0; i<numDims; i++) {
              qpVal += opField(cell,qp,i)*opField(cell,qp,i);
            }
          }
          else qpVal = opField(cell,qp);
          this->global_response[0] += qpVal * weights(cell,qp);
        }
      }
      else {
        for (std::size_t qp=0; qp < numQPs; ++qp) {
          qpVal = 0.0;
          if(bRetFieldIsVector) {
            for(std::size_t i=0; i<numDims; i++) {
              qpVal += retField(cell,qp,i)*retField(cell,qp,i);
            }
          }
          else qpVal = retField(cell,qp);
          this->global_response[0] += qpVal * weights(cell,qp);
        }
      }
      this->global_response[0] /= cellVol;

      // set g[1] = value of the field operated on at the current cell (avg)
      this->global_response[1] = opVal;

      // set g[2+] = average qp coordinate values of the current cell
      for(std::size_t i=0; i<numDims; i++) {
        this->global_response[i+2] = 0.0;
        for (std::size_t qp=0; qp < numQPs; ++qp) 
	  this->global_response[i+2] += coordVec(cell,qp,i);
	this->global_response[i+2] /= numQPs;
      }
    }

  } // end of loop over cells

  // No local scattering
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseFieldValue<EvalT, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  int indexToMax = 1;
  ScalarT max = this->global_response[indexToMax];
  Teuchos::EReductionType reductType;
  if (operation == "Maximize")
    reductType = Teuchos::REDUCE_MAX;
  else
    reductType = Teuchos::REDUCE_MIN;

  Teuchos::RCP< Teuchos::ValueTypeSerializer<int,ScalarT> > serializer =
    workset.serializerManager.template getValue<EvalT>();

  // Compute contributions across processors
  Teuchos::reduceAll(
    *workset.comm, *serializer, reductType, 1, 
    &this->global_response[indexToMax], &max);

  int procToBcast;
  if( this->global_response[indexToMax] == max ) 
    procToBcast = workset.comm->getRank();
  else procToBcast = -1;

  int winner;
  Teuchos::reduceAll(
    *workset.comm, Teuchos::REDUCE_MAX, 1, &procToBcast, &winner);
  Teuchos::broadcast(
    *workset.comm, *serializer, winner, this->global_response.size(), 
    &this->global_response[0]);

  // Do global scattering
  if (workset.comm->getRank() == winner)
    QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::setNodeID(max_nodeID);
  QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::postEvaluate(workset);
}

// **********************************************************************
template<typename EvalT,typename Traits>
Teuchos::RCP<const Teuchos::ParameterList>
QCAD::ResponseFieldValue<EvalT,Traits>::getValidResponseParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
     	rcp(new Teuchos::ParameterList("Valid ResponseFieldValue Params"));
  Teuchos::RCP<const Teuchos::ParameterList> baseValidPL =
    QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::getValidResponseParameters();
  validPL->setParameters(*baseValidPL);

  validPL->set<string>("Name", "", "Name of response function");
  validPL->set<int>("Phalanx Graph Visualization Detail", 0, "Make dot file to visualize phalanx graph");
  validPL->set<string>("Type", "", "Response type");
  validPL->set<string>("Operation", "Maximize", "Operation to perform");
  validPL->set<string>("Operation Field Name", "", "Scalar field to perform operation on");
  validPL->set<string>("Operation Vector Field Name", "", "Vector field to perform operation on");
  validPL->set<string>("Return Field Name", "<operation field name>",
		       "Scalar field to return value from");
  validPL->set<string>("Return Vector Field Name", "<operation vector field name>",
		       "Vector field to return value from");

  validPL->set<string>("Operation Domain", "box", "Region to perform operation: 'box' or 'element block'");
  validPL->set<bool>("Operate on x-component", true, 
		     "Whether to perform operation on x component of vector field");
  validPL->set<bool>("Operate on y-component", true, 
		     "Whether to perform operation on y component of vector field");
  validPL->set<bool>("Operate on z-component", true, 
		     "Whether to perform operation on z component of vector field");

  validPL->set<double>("x min", 0.0, "Box domain minimum x coordinate");
  validPL->set<double>("x max", 0.0, "Box domain maximum x coordinate");
  validPL->set<double>("y min", 0.0, "Box domain minimum y coordinate");
  validPL->set<double>("y max", 0.0, "Box domain maximum y coordinate");
  validPL->set<double>("z min", 0.0, "Box domain minimum z coordinate");
  validPL->set<double>("z max", 0.0, "Box domain maximum z coordinate");

  validPL->set<string>("Element Block Name", "", "Element block name that specifies domain");

  return validPL;
}

// **********************************************************************

