/**
This file is part of tumorcode project.
(http://www.uni-saarland.de/fak7/rieger/homepage/research/tumor/tumor.html)

Copyright (C) 2016  Michael Welter and Thierry Fredrich

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "python_helpers.h"
//#include "numpy.hpp"
#include "pylatticedata.h"

#include "shared-objects.h"
#include "vessels3d.h"

#include "common/calcflow.h" //at least for remap_keys


namespace py = boost::python;
namespace np = boost::python::numpy;

namespace murray
{
/** @brief Get radii for the vesseltree
 * 
 * analyze only where there is a intersection
 */
#define max_edges 4


class MurrayCoeffs
{
  CompressedFlowNetwork &nw; // stores edges and properties
  CompressedRows nbs;
  FlArray   press;
  FlArray   radius;
  const FlReal &alpha;
  
  FlArray radius_mother;
  FlArray radius_daughther1;
  FlArray radius_daughther2;
//   std::vector<double> radius_mother;
//   std::vector<double> radius_daughther1;
//   std::vector<double> radius_daughther2;
  // get neighboring info i.e. node -> #neighbors, neighbor[k] -> edge, node
  int GetNeighborCount(int i) const { return nbs.row_length(i); }
  NodeNeighbor<int,int> GetNeighbor(int i, int k) const
  {
    int id_of_edge = nbs.column_number(i, k);
    const my::eqpair<int> &edge = nw.edges[id_of_edge];
    return make_node_neighbor(edge.first==i ? edge.second : edge.first, id_of_edge);
  }
  my::eqpair<int>   GetEdge(int i) const                { return nw.edges[i]; }
  FlReal            GetPress(int i) const               { return press[i]; }
  FlReal	    GetRadius(int i) const		{ return radius[i];}
  
public:
  MurrayCoeffs(CompressedFlowNetwork &fl,const VesselList3d *vl, const FlReal &alpha, const FlArray &radius);
  FlArray getDaughter1(){return radius_daughther1;};
  FlArray getDaughter2(){return radius_daughther2;};
  FlArray getMother(){return radius_mother;};
};

MurrayCoeffs::MurrayCoeffs(CompressedFlowNetwork &fl_,const VesselList3d *vl, const FlReal &alpha_, const FlArray &radius_) : nw(fl_), alpha(alpha_), radius(radius_)
{
  int ecnt = nw.edges.size();
  int ncnt = nw.num_vertices();
  press.resize(ncnt);
  for(int i=0;i<vl->GetNCount();++i)
  {
    const VesselNode *nd= vl->GetNode(i);
    int id= nw.org2new_vertex[nd->Index()];
    press[id] = nd->press;
  }
  radius_daughther1.resize(ncnt);
  radius_daughther2.resize(ncnt);
  radius_mother.resize(ncnt);
#ifdef DEBUG
  printf("nw.press.size(): %i\n", nw.press.size());
  printf("ncnt: %i\n", ncnt);
  //myAssert(nw.press.size()==ncnt);//if there are filter uncirculated vessels this doesn't make sense
  myAssert(nw.rad.size()==ecnt);
#endif
  std::vector<unsigned int> col_counts(ncnt);
  for (int i=0; i<ecnt; ++i)
  {
    const my::eqpair<int> &e = nw.edges[i];
    col_counts[e.first]++;
    col_counts[e.second]++;
  }
  nbs.reinit(ncnt, col_counts);
  nbs.allow_identical_edges = false;
  for (int i=0; i<ecnt; ++i)
  {
    const my::eqpair<int> &e = nw.edges[i];
    nbs.add(e.first, i);
    nbs.add(e.second, i);
  }
}
//serial working version
#if 1 // not H5Cpp ready
static py::object get_Murray_scale(const string fn, const string path)
{
  //h5::Group vesselgroup = PythonToCppGroup(vess_grp_obj);
  H5::H5File *readInFile = new H5::H5File(fn, H5F_ACC_RDONLY);
  H5::Group vesselgroup = readInFile->openGroup(path);
  std::shared_ptr<VesselList3d> vl;
  vl = ReadVesselList3d(vesselgroup, make_ptree("filter", true));
  int ncnt = vl.get()->GetNCount();

  FlArray radius_mother;
  radius_mother.resize(ncnt);
  FlArray radius_daughther1;
  radius_daughther1.resize(ncnt);
  FlArray radius_daughther2;
  radius_daughther2.resize(ncnt);
  for(int i = 0;i<ncnt; ++i)
  {
    VesselNode* nd = vl.get()->GetNode(i);
    if( nd->Count() == 2) // for strait lines
    {
      uint current_daughter=0;
      if( nd->GetEdge(0)->IsArtery() and nd->GetEdge(1)->IsArtery() )
      {
	for(int k = 0;k<2;++k)
	{
	  //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
	  VesselNode* neighbor_nd;
	  neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
	  if( nd->press < neighbor_nd->press)
	  {
	    radius_mother[i] = nd->GetEdge(k)->r;
	  }
	  else
	  {
	    if(current_daughter == 0)
	    {
	      radius_daughther1[i] = nd->GetEdge(k)->r;
	      radius_daughther2[i] = nd->GetEdge(k)->r; //little hackish
	      current_daughter++;
	    }
	    //should not happen in the straight case!
// 	    if(current_daughter == 1)
// 	    {
// 	      radius_daughther2[i] = nd->GetEdge(k)->r;
// 	      current_daughter++;
// 	    }
	  }
	}
      }
      //venous part
      if( nd->GetEdge(0)->IsVein() and nd->GetEdge(1)->IsVein() )
      {
	for(int k = 0;k<2;++k)
	{
	  //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
	  VesselNode* neighbor_nd;
	  neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
	  if( nd->press > neighbor_nd->press)
	  {
	    if(current_daughter == 0)
	    {
	      radius_daughther1[i] = nd->GetEdge(k)->r;
	      radius_daughther2[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
// 	    if(current_daughter == 1)
// 	    {
// 	      radius_daughther2[i] = nd->GetEdge(k)->r;
// 	      current_daughter++;
// 	    }
	  }
	  else
	  {
	    radius_mother[i] = nd->GetEdge(k)->r;
	  }
	}
      }
    }

    if( nd->Count() == 3 ) //for the Y-intersections
    {
      uint current_daughter=0;
      if( nd->GetEdge(0)->IsArtery() and nd->GetEdge(1)->IsArtery() and nd->GetEdge(2)->IsArtery())
      {
	for(int k = 0;k<3;++k)
	{
	  //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
	  VesselNode* neighbor_nd;
	  neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
	  if( nd->press < neighbor_nd->press)
	  {
	    radius_mother[i] = nd->GetEdge(k)->r;
	  }
	  else
	  {
	    if(current_daughter == 0)
	    {
	      radius_daughther1[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
	    if(current_daughter == 1)
	    {
	      radius_daughther2[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
	  }
	}
      }
      //venous part
      if( nd->GetEdge(0)->IsVein() and nd->GetEdge(1)->IsVein() and nd->GetEdge(2)->IsVein())
      {
	for(int k = 0;k<3;++k)
	{
	  //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
	  VesselNode* neighbor_nd;
	  neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
	  if( nd->press > neighbor_nd->press)
	  {
	    if(current_daughter == 0)
	    {
	      radius_daughther1[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
	    if(current_daughter == 1)
	    {
	      radius_daughther2[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
	  }
	  else
	  {
	    radius_mother[i] = nd->GetEdge(k)->r;
	  }
	}
      }
    }
  }
#if BOOST_VERSION>106300
  //Py_ssize_t ndims[] = { 3, ncnt };
  py::tuple shape = py::make_tuple(3,ncnt);
  np::dtype dtype = np::dtype::get_builtin<float>();
  np::ndarray murray = np::zeros(shape, dtype);

  for(int i=0;i<ncnt;++i)
  {
    murray[0][i] = radius_daughther1[i];
    murray[1][i] = radius_daughther2[i];
    murray[2][i] = radius_mother[i];
  }
  return murray;
#else
  Py_ssize_t ndims[] = { 3, ncnt };
  np::arrayt<float>murray = np::zeros(2, ndims, np::getItemtype<float>());

  for(int i=0;i<ncnt;++i)
  {
    murray(0,i) = radius_daughther1[i];
    murray(1,i) = radius_daughther2[i];
    murray(2,i) = radius_mother[i];
  }
  return py::object(murray);
#endif
}

#endif

#if 0 //not H5Cpp ready
#if BOOST_VERSION>106300
#else
//serial working version
static py::object get_Murray2(const py::object &vess_grp_obj)
{
  h5::Group vesselgroup = PythonToCppGroup(vess_grp_obj);
  std::auto_ptr<VesselList3d> vl;
  vl = ReadVesselList3d(vesselgroup, make_ptree("filter", true));
  int ncnt = vl.get()->GetNCount();

  FlArray radius_mother;
  radius_mother.resize(ncnt);
  FlArray radius_daughther1;
  radius_daughther1.resize(ncnt);
  FlArray radius_daughther2;
  radius_daughther2.resize(ncnt);
  for(int i = 0;i<ncnt; ++i)
  {
    VesselNode* nd = vl.get()->GetNode(i);
    if( nd->Count() == 2) // for strait lines
    {
      uint current_daughter=0;
      if( nd->GetEdge(0)->IsArtery() and nd->GetEdge(1)->IsArtery() )
      {
	for(int k = 0;k<2;++k)
	{
	  //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
	  VesselNode* neighbor_nd;
	  neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
	  if( nd->press < neighbor_nd->press)
	  {
	    radius_mother[i] = nd->GetEdge(k)->r;
	  }
	  else
	  {
	    if(current_daughter == 0)
	    {
	      radius_daughther1[i] = nd->GetEdge(k)->r;
	      radius_daughther2[i] = nd->GetEdge(k)->r; //little hackish
	      current_daughter++;
	    }
	    //should not happen in the straight case!
// 	    if(current_daughter == 1)
// 	    {
// 	      radius_daughther2[i] = nd->GetEdge(k)->r;
// 	      current_daughter++;
// 	    }
	  }
	}
      }
      //venous part
      if( nd->GetEdge(0)->IsVein() and nd->GetEdge(1)->IsVein() )
      {
	for(int k = 0;k<2;++k)
	{
	  //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
	  VesselNode* neighbor_nd;
	  neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
	  if( nd->press < neighbor_nd->press)
	  {
	    if(current_daughter == 0)
	    {
	      radius_daughther1[i] = nd->GetEdge(k)->r;
	      radius_daughther2[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
// 	    if(current_daughter == 1)
// 	    {
// 	      radius_daughther2[i] = nd->GetEdge(k)->r;
// 	      current_daughter++;
// 	    }
	  }
	  else
	  {
	    radius_mother[i] = nd->GetEdge(k)->r;
	  }
	}
      }
    }

    if( nd->Count() == 3 ) //for the Y-intersections
    {
      uint current_daughter=0;
      if( nd->GetEdge(0)->IsArtery() and nd->GetEdge(1)->IsArtery() and nd->GetEdge(2)->IsArtery())
      {
	for(int k = 0;k<3;++k)
	{
	  //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
	  VesselNode* neighbor_nd;
	  neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
	  if( nd->press < neighbor_nd->press)
	  {
	    radius_mother[i] = nd->GetEdge(k)->r;
	  }
	  else
	  {
	    if(current_daughter == 0)
	    {
	      radius_daughther1[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
	    if(current_daughter == 1)
	    {
	      radius_daughther2[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
	  }
	}
      }
      //venous part
      if( nd->GetEdge(0)->IsVein() and nd->GetEdge(1)->IsVein() and nd->GetEdge(2)->IsVein())
      {
	for(int k = 0;k<3;++k)
	{
	  //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
	  VesselNode* neighbor_nd;
	  neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
	  if( nd->press < neighbor_nd->press)
	  {
	    if(current_daughter == 0)
	    {
	      radius_daughther1[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
	    if(current_daughter == 1)
	    {
	      radius_daughther2[i] = nd->GetEdge(k)->r;
	      current_daughter++;
	    }
	  }
	  else
	  {
	    radius_mother[i] = nd->GetEdge(k)->r;
	  }
	}
      }
    }
  }
   Py_ssize_t ndims[] = { 3, ncnt };
   np::arrayt<float>murray = np::zeros(2, ndims, np::getItemtype<float>());

  for(int i=0;i<ncnt;++i)
  {
    murray(0,i) = radius_daughther1[i];
    murray(1,i) = radius_daughther2[i];
    murray(2,i) = radius_mother[i];
  }
  return py::object(murray);
}
#endif
#endif

#if 1 //not H5Cpp ready
#if BOOST_VERSION>106300
//try parallel here!!!

static py::object get_Murray2_p(const string fn, const string path)
{
  H5::H5File *readInFile = new H5::H5File(fn, H5F_ACC_RDONLY);
  H5::Group vesselgroup = readInFile->openGroup(path);
  //double alpha = py::extract<double>(murrayalpha);
  //h5::Group vesselgroup = PythonToCppGroup(vess_grp_obj);
  std::shared_ptr<VesselList3d> vl;
  vl = ReadVesselList3d(vesselgroup, make_ptree("filter", true));
  int ncnt = vl.get()->GetNCount();

  //venous
  std::vector<double> radius_mother_v;
  std::vector<double> radius_daughther1_v;
  std::vector<double> radius_daughther2_v;
  //arterial
  std::vector<double> radius_mother_a;
  std::vector<double> radius_daughther1_a;
  std::vector<double> radius_daughther2_a;
  
  for(int i = 0;i<ncnt; ++i)
  {
    VesselNode* nd = vl.get()->GetNode(i);

    if( nd->Count() == 3 ) //for the Y-intersections
    {
      std::vector<double> radii;
      for(int k=0;k<3;++k)
      {
        radii.push_back(nd->GetEdge(k)->r);
      }
      std::sort (radii.begin(),radii.end());
      if(nd->GetEdge(0)->IsArtery() and nd->GetEdge(1)->IsArtery() and nd->GetEdge(2)->IsArtery())
      {
        radius_mother_a.push_back(radii[2]);
        radius_daughther1_a.push_back(radii[0]);
        radius_daughther2_a.push_back(radii[1]);
      }
      if(nd->GetEdge(0)->IsVein() and nd->GetEdge(1)->IsVein() and nd->GetEdge(2)->IsVein())
      {
        radius_mother_v.push_back(radii[2]);
        radius_daughther1_v.push_back(radii[0]);
        radius_daughther2_v.push_back(radii[1]);
      }
    }
  }
//   Py_ssize_t ndims[] = { 3, ncnt };
//   np::arrayt<float>murray_v = np::zeros(2, ndims, np::getItemtype<float>());
//   np::arrayt<float>murray_a = np::zeros(2, ndims, np::getItemtype<float>());
  py::tuple murray_shape = py::make_tuple(3,ncnt);
  np::ndarray murray_v = np::zeros(murray_shape,np::dtype::get_builtin<float>());
  np::ndarray murray_a = np::zeros(murray_shape,np::dtype::get_builtin<float>());
   
  for(int i=0;i<radius_mother_v.size();++i)
  {
    murray_v[0][i] = radius_daughther1_v[i];
    murray_v[1][i] = radius_daughther2_v[i];
    murray_v[2][i] = radius_mother_v[i];
  }
  for(int i=0;i<radius_mother_a.size();++i)
  {
    murray_a[0][i] = radius_daughther1_a[i];
    murray_a[1][i] = radius_daughther2_a[i];
    murray_a[2][i] = radius_mother_a[i];
  }
  return py::make_tuple(py::object(murray_v),py::object(murray_a));
}
#else
//try parallel here!!!
static py::object get_Murray2_p(const string fn, const string path)
{
  H5::H5File readInFile = H5::H5File(fn, H5F_ACC_RDONLY);
  H5::Group vesselgroup = readInFile.openGroup(path);
//static py::object get_Murray2_p(const py::object &vess_grp_obj)
//{
  //double alpha = py::extract<double>(murrayalpha);
//  h5::Group vesselgroup = PythonToCppGroup(vess_grp_obj);
  std::shared_ptr<VesselList3d> vl;
  vl = ReadVesselList3d(vesselgroup, make_ptree("filter", true));
  int ncnt = vl.get()->GetNCount();

  //venous
  std::vector<double> radius_mother_v;
  std::vector<double> radius_daughther1_v;
  std::vector<double> radius_daughther2_v;
  //arterial
  std::vector<double> radius_mother_a;
  std::vector<double> radius_daughther1_a;
  std::vector<double> radius_daughther2_a;
  
  for(int i = 0;i<ncnt; ++i)
  {
    VesselNode* nd = vl.get()->GetNode(i);
#if 0
    if( nd->Count() == 2) // for strait lines
    {
      uint current_daughter=0;
      if( nd->GetEdge(0)->IsArtery() and nd->GetEdge(1)->IsArtery() )
      {
        for(int k = 0;k<2;++k)
        {
          //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
          VesselNode* neighbor_nd;
          neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
          if( nd->press < neighbor_nd->press)
          {
            radius_mother[i] = nd->GetEdge(k)->r;
          }
          else
          {
            if(current_daughter == 0)
            {
              radius_daughther1[i] = nd->GetEdge(k)->r;
              radius_daughther2[i] = nd->GetEdge(k)->r; //little hackish
              current_daughter++;
            }
            //should not happen in the straight case!
      // 	    if(current_daughter == 1)
      // 	    {
      // 	      radius_daughther2[i] = nd->GetEdge(k)->r;
      // 	      current_daughter++;
      // 	    }
          }
        }
      }
      //venous part
      if( nd->GetEdge(0)->IsVein() and nd->GetEdge(1)->IsVein() )
      {
        for(int k = 0;k<2;++k)
        {
          //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
          VesselNode* neighbor_nd;
          neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
          if( nd->press < neighbor_nd->press)
          {
            if(current_daughter == 0)
            {
              radius_daughther1[i] = nd->GetEdge(k)->r;
              radius_daughther2[i] = nd->GetEdge(k)->r;
              current_daughter++;
            }
      // 	    if(current_daughter == 1)
      // 	    {
      // 	      radius_daughther2[i] = nd->GetEdge(k)->r;
      // 	      current_daughter++;
      // 	    }
          }
          else
          {
            radius_mother[i] = nd->GetEdge(k)->r;
          }
        }
      }
    }
#endif

    if( nd->Count() == 3 ) //for the Y-intersections
    {
      std::vector<double> radii;
      for(int k=0;k<3;++k)
      {
        radii.push_back(nd->GetEdge(k)->r);
      }
      std::sort (radii.begin(),radii.end());
      if(nd->GetEdge(0)->IsArtery() and nd->GetEdge(1)->IsArtery() and nd->GetEdge(2)->IsArtery())
      {
        radius_mother_a.push_back(radii[2]);
        radius_daughther1_a.push_back(radii[0]);
        radius_daughther2_a.push_back(radii[1]);
      }
      if(nd->GetEdge(0)->IsVein() and nd->GetEdge(1)->IsVein() and nd->GetEdge(2)->IsVein())
      {
        radius_mother_v.push_back(radii[2]);
        radius_daughther1_v.push_back(radii[0]);
        radius_daughther2_v.push_back(radii[1]);
      }
//       //arterial part
//       uint current_daughter=0;
//       if( nd->GetEdge(0)->IsArtery() and nd->GetEdge(1)->IsArtery() and nd->GetEdge(2)->IsArtery())
//       {
//         for(int k = 0;k<3;++k)
//         {
//           //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
//           VesselNode* neighbor_nd;
//           neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
//           if( nd->press < neighbor_nd->press)
//           {
//             radius_mother[i] = nd->GetEdge(k)->r;
//           }
//           else
//           {
//             if(current_daughter == 0)
//             {
//               radius_daughther1[i] = nd->GetEdge(k)->r;
//               current_daughter++;
//             }
//             if(current_daughter == 1)
//             {
//               radius_daughther2[i] = nd->GetEdge(k)->r;
//               current_daughter++;
//             }
//           }
//         }
//       }
//       //venous part
//       if( nd->GetEdge(0)->IsVein() and nd->GetEdge(1)->IsVein() and nd->GetEdge(2)->IsVein())
//       {
//         for(int k = 0;k<3;++k)
//         {
//           //upstream = mother const int  imd = ee[0]==theconsideredNode ? ee[1] : ee[0];
//           VesselNode* neighbor_nd;
//           neighbor_nd = nd->GetEdge(k)->GetNode(0) == nd ? nd->GetEdge(k)->GetNode(1):nd->GetEdge(k)->GetNode(0);
//           if( nd->press < neighbor_nd->press)
//           {
//             if(current_daughter == 0)
//             {
//               radius_daughther1[i] = nd->GetEdge(k)->r;
//               current_daughter++;
//             }
//             if(current_daughter == 1)
//             {
//               radius_daughther2[i] = nd->GetEdge(k)->r;
//               current_daughter++;
//             }
//           }
//           else
//           {
//             radius_mother[i] = nd->GetEdge(k)->r;
//           }
//         }
//       }
    }
  }
  Py_ssize_t ndims[] = { 3, ncnt };
  np::arrayt<float>murray_v = np::zeros(2, ndims, np::getItemtype<float>());
  np::arrayt<float>murray_a = np::zeros(2, ndims, np::getItemtype<float>());
   
  for(int i=0;i<radius_mother_v.size();++i)
  {
    murray_v(0,i) = radius_daughther1_v[i];
    murray_v(1,i) = radius_daughther2_v[i];
    murray_v(2,i) = radius_mother_v[i];
  }
  for(int i=0;i<radius_mother_a.size();++i)
  {
    murray_a(0,i) = radius_daughther1_a[i];
    murray_a(1,i) = radius_daughther2_a[i];
    murray_a(2,i) = radius_mother_a[i];
  }
  return py::make_tuple(py::object(murray_v),py::object(murray_a));
}
#endif
#endif

}//namespace murray
void export_get_Murray()
{
  //py::def("get_Murray", murray::get_Murray);
  //py::def("get_Murray2", murray::get_Murray2);
  
  //these 2 were used
  py::def("get_Murray2", murray::get_Murray2_p);
  py::def("get_Murray_scale", murray::get_Murray_scale);
}
