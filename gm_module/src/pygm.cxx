// boost python related
#include <boost/python/detail/wrap_python.hpp>
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/exception_translator.hpp>
#include <numpy/arrayobject.h>
#include <numpy/noprefix.h>

// vigra numpy array converters
#include <vigra/numpy_array.hxx>
#include <vigra/numpy_array_converters.hxx>

// standart c++ headers (whatever you need (string and vector are just examples))
#include <string>
#include <vector>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include <boost/python/list.hpp>
#include <boost/python/tuple.hpp>


// this functions show how to loop over an
// entire image and do something with each pixel
// and store the result in a new image


//typedef vigra::NumpyArray<2,uint32_t> ArrayType;
//typedef ArrayType::difference_type ArrayShape;

// vigra::NumpyArray<2,uint32_t>::difference_type

struct edge // key type for map
{
    uint32_t v1;
    uint32_t v2;
    edge(uint32_t sp1, uint32_t sp2) : v1(sp1), v2(sp2) {}
};

struct edgeData
{
    size_t volume;
    size_t circumference;
    float  avg_value;
    float  avg_gradient;
    float  std_value;
    float  std_gradient;
    vigra::NumpyArray<2,uint32_t> face;
    size_t size1;
    size_t size2;

    size_t counter;
    edgeData() : volume(0), circumference(0), avg_gradient(0), avg_value(0), std_gradient(0), std_value(0), size1(0), size2(0), counter(0) {} // initialise all to zero
};
bool operator==(edge const& p1, edge const& p2) // speicify equalit-relation for keys
{
    return p1.v1 == p2.v1 && p1.v2 == p2.v2;
    }

std::size_t hash_value(edge const& p) // overload hash-fu for custom key-type
{
    std::size_t seed = 0;
    boost::hash_combine(seed, p.v1);
    boost::hash_combine(seed, p.v2);
    return seed;
}

typedef boost::unordered_map<edge,edgeData> EdgeMapType;

boost::python::tuple adjGraph(vigra::NumpyArray<3,uint32_t> spixels, vigra::NumpyArray<3, float> data, bool print)
{
    size_t dimX=spixels.shape(0);
    size_t dimY=spixels.shape(1);
    size_t dimZ=spixels.shape(2);
    uint32_t sp1,sp2;
    edge key(0,0);
    size_t num_e = 0;
    EdgeMapType edges; // hash table for storing edges: { key: sp 1 | value: sp 2 } and sp 1 > sp 2! Otherwise not unique!
    // 3-fold scanning loop to detect neigbourhood-relations and mark them in the adjecancy-list/map
    for(size_t z=0; z<dimZ; ++z)
    {
        for(size_t y=0; y<dimY; ++y)
        {
            for(size_t x=0; x<dimX; ++x)
            {
                if(print) std::cout<<"x="<<int(x)<<" y="<<int(y)<<" z="<<int(z)<<": ";
                if((x<dimX-1) && (spixels(x,y,z) != spixels(x+1,y,z))) // 1. check bounds 2. if two adjecant voxel have different classification
                {
                    sp1 = std::min(spixels(x,y,z),spixels(x+1,y,z));
                    sp2 = std::max(spixels(x,y,z),spixels(x+1,y,z));
                    key = edge(sp1,sp2);
                    if(print)std::cout<<"found ("<<sp1<<"|"<<sp2<<"), ";
                    if (edges.find(key) == edges.end()) // if this edge was not added before
                    {
                        edges[key] = edgeData();
                        num_e++;
                    }
                    edges[key].volume += 2;
                }
                if((y<dimZ-1) && (spixels(x,y,z) != spixels(x,y+1,z))) // 1. check bounds 2. if two adjecant voxel have different classification
                {
                    sp1 = std::min(spixels(x,y,z),spixels(x,y+1,z));
                    sp2 = std::max(spixels(x,y,z),spixels(x,y+1,z));
                    key = edge(sp1,sp2);
                    if(print)std::cout<<"found ("<<sp1<<"|"<<sp2<<"), ";
                    if (edges.find(key) == edges.end()) // if this edge was not added before
                    {
                        edges[key] = edgeData();
                        num_e++;
                    }
                    edges[key].volume += 2;
                }
                if((z<dimZ-1) && (spixels(x,y,z) != spixels(x,y,z+1))) // 1. check bounds 2. if two adjecant voxel have different classification
                {
                    sp1 = std::min(spixels(x,y,z),spixels(x,y,z+1));
                    sp2 = std::max(spixels(x,y,z),spixels(x,y,z+1));
                    key = edge(sp1,sp2);
                    if(print)std::cout<<"found ("<<sp1<<"|"<<sp2<<")";
                    if (edges.find(key) == edges.end()) // if this edge was not added before
                    {
                        edges[key] = edgeData();
                        num_e++;
                    }
                    edges[key].volume += 2;
                }
                if(print)std::cout<<std::endl;
            }
        }
    }

    BOOST_FOREACH(EdgeMapType::value_type &p, edges)
    {
        p.second.face = vigra::NumpyArray<2,uint32_t>(vigra::NumpyArray<2,uint32_t>::difference_type(p.second.volume,3));  // Initialise list of voxels to correct lenght and for 3 coordinates
        if(print) std::cout<<"Initialise array for ("<<int(p.first.v1)<<" | "<<int(p.first.v2)<<") to "<<int(p.second.volume)<<"\n";
    }

    EdgeMapType::iterator it;
    edgeData * value;
    for(size_t z=0; z<dimZ; ++z)
    {
        for(size_t y=0; y<dimY; ++y)
        {
            for(size_t x=0; x<dimX; ++x)
            {
                if(print) std::cout<<"xyz="<<int(x)<<"-"<<int(y)<<"-"<<int(z)<<":";
                if((x<dimX-1) && (spixels(x,y,z) != spixels(x+1,y,z))) // 1. check bounds 2. if two adjecant voxel have different classification
                {
                    sp1 = std::min(spixels(x,y,z),spixels(x+1,y,z));
                    sp2 = std::max(spixels(x,y,z),spixels(x+1,y,z));
                    key = edge(sp1,sp2);
                    value = &(edges.find(key)->second);

                    value->face(value->counter, 0) = x;
                    value->face(value->counter, 1) = y;
                    value->face(value->counter, 2) = z;   value->counter++;
                    value->face(value->counter, 0) = x+1;;
                    value->face(value->counter, 1) = y;
                    value->face(value->counter, 2) = z;   value->counter++;

                    value->avg_value    += (data(x,y,z) + data(x+1,y,z))*2/value->volume;
                    value->avg_gradient += abs(data(x,y,z) - data(x+1,y,z))*2/value->volume;
                }
                if((y<dimY-1) && (spixels(x,y,z) != spixels(x,y+1,z))) // 1. check bounds 2. if two adjecant voxel have different classification
                {
                    sp1 = std::min(spixels(x,y,z),spixels(x,y+1,z));
                    sp2 = std::max(spixels(x,y,z),spixels(x,y+1,z));
                    key = edge(sp1,sp2);
                    value = &(edges.find(key)->second);

                    value->face(value->counter, 0) = x;
                    value->face(value->counter, 1) = y;
                    value->face(value->counter, 2) = z;   value->counter++;
                    value->face(value->counter, 0) = x;
                    value->face(value->counter, 1) = y+1;
                    value->face(value->counter, 2) = z;   value->counter++;

                    value->avg_value    += (data(x,y,z) + data(x,y+1,z))*2/value->volume;
                    value->avg_gradient += abs(data(x,y,z) - data(x,y+1,z))*2/value->volume;
                }
                if((z<dimZ-1) && (spixels(x,y,z) != spixels(x,y,z+1))) // 1. check bounds 2. if two adjecant voxel have different classification
                {
                    sp1 = std::min(spixels(x,y,z),spixels(x,y,z+1));
                    sp2 = std::max(spixels(x,y,z),spixels(x,y,z+1));
                    key = edge(sp1,sp2);
                    value = &(edges.find(key)->second);
                    printf("\nadd x=%d to face(%d|0) ", int(x), int(value->counter));
                    value->face(value->counter, 0) = x;
                    printf("in Map is the value %d, counter=%d\n", value->face(value->counter, 0), int(value->counter));

                    printf("add y=%d to face(%d|1) ", int(y), int(value->counter));
                    value->face(value->counter, 1) = y;
                    printf("in Map is the value %d, counter=%d\n", value->face(value->counter, 1), int(value->counter));

                    printf("add z=%d to face(%d|2) ", int(z), int(value->counter));
                    value->face(value->counter, 2) = z;   value->counter++;
                    printf("in Map is the value %d, counter=%d\n", value->face(value->counter-1, 2), int(value->counter));

                    printf("add x=%d to face(%d|0) ", int(x), int(value->counter));
                    value->face(value->counter, 0) = int(x);
                    printf("in Map is the value %d, counter=%d\n", value->face(value->counter, 0), int(value->counter));

                    printf("add y=%d to face(%d|1) ", int(y), int(value->counter));
                    value->face(value->counter, 1) = y;
                    printf("in Map is the value %d, counter=%d\n", value->face(value->counter, 1), int(value->counter));

                    printf("add z=%d to face(%d|2) ", int(z)+1, int(value->counter));
                    value->face(value->counter, 2) = z+1; value->counter++;
                    printf("in Map is the value %d, counter=%d\n", value->face(value->counter-1, 2), int(value->counter));
                    if(print)std::cout<<"added coordinates, ";

                    value->avg_value    += (data(x,y,z) + data(x,y,z+1))*2/value->volume;
                    value->avg_gradient += abs(data(x,y,z) - data(x,y,z+1))*2/value->volume;
                }
                if(print)std::cout<<std::endl;
            }
        }
    }

    vigra::NumpyArray<2,float> edges_arr;
    edges_arr.reshape(vigra::Shape2(num_e,5));
    boost::python::list face_list;
    size_t i = 0;
    BOOST_FOREACH(EdgeMapType::value_type p, edges)
    {
        if(print)std::cout<<"adding ("<<p.first.v1<<"|"<<p.first.v2<<")\n";
        edges_arr(i,0) = p.first.v1;
        edges_arr(i,1) = p.first.v2;
        edges_arr(i,2) = p.second.volume;
        edges_arr(i,3) = p.second.avg_value;
        edges_arr(i,4) = p.second.avg_gradient;
        face_list.append(p.second.face);
        i++;
    }
    printf("edge is (0|1): average=%f, volume=%d, gradien=%f, facelist\n", edges[edge(0,1)].avg_value, int(edges[edge(0,1)].volume), edges[edge(0,1)].avg_gradient);
    printf("edge is (%1.f|%1.f): average=%f, volume=%1.f, gradien=%f, facelist\n", edges_arr(0,0),edges_arr(0,1),edges_arr(0,3),edges_arr(0,2),edges_arr(0,4));
    printf("edgenum is %d\n", int(num_e));

    return boost::python::make_tuple(edges_arr,face_list, num_e);
}

void export_adjGraph()
{
    // Do not change next 4 lines
    import_array();
    vigra::import_vigranumpy();
    boost::python::numeric::array::set_module_and_type("numpy", "ndarray");
    boost::python::docstring_options docstringOptions(true,true,false);
    // No not change 4 line above

    // export the function to python
    boost::python::def("adjGraph", vigra::registerConverters(&adjGraph) ,
                       (
                           boost::python::arg("spixels, print")
                       ),
                       "Args:\n\n"
                       "   spixels : 3-d array of superpixels (SP), denoted by different uint32-values \n\n"
                       "   print   : turn on printing of processing information (for debugging)\n\n"
                       "returns: \n\n"
                       "   edges   : vector of adjacent SP (sp 1| sp 2) in a 6-neighbourhood. The number of the first SP is always > the number of the second SP\n\n"
                       "   #edges  : number of found edges\n\n"
                      );
}