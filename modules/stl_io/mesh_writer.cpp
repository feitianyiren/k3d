// K-3D
// Copyright (c) 1995-2009, Timothy M. Shead
//
// Contact: tshead@k-3d.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

/** \file
	\author Tim Shead (tshead@k-3d.com)
	\author Bart Janssens (bart.janssens@lid.kviv.be)
*/

#include <k3d-i18n-config.h>
#include <k3d-version-config.h>
#include <k3dsdk/document_plugin_factory.h>
#include <k3dsdk/fstream.h>
#include <k3dsdk/mesh_operations.h>
#include <k3dsdk/mesh_sink.h>
#include <k3dsdk/node.h>
#include <k3dsdk/polyhedron.h>

#include <boost/scoped_ptr.hpp>

namespace module
{

namespace stl
{

namespace io
{

/////////////////////////////////////////////////////////////////////////////
// mesh_writerio

class mesh_writer :
	public k3d::mesh_sink<k3d::node >
{
	typedef k3d::mesh_sink<k3d::node > base;

public:
	mesh_writer(k3d::iplugin_factory& Factory, k3d::idocument& Document) :
		base(Factory, Document),
		m_file(init_owner(*this) + init_name("file") + init_label(_("File")) + init_description(_("Output file")) + init_value(k3d::filesystem::path()) + init_path_mode(k3d::ipath_property::WRITE) + init_path_type("stl_files"))
	{
		m_file.changed_signal().connect(sigc::mem_fun(*this, &mesh_writer::on_write_file));
		m_input_mesh.changed_signal().connect(sigc::mem_fun(*this, &mesh_writer::on_write_file));
	}

	void on_write_file(k3d::iunknown*)
	{
		const k3d::filesystem::path path = m_file.pipeline_value();
		const k3d::mesh* const mesh = m_input_mesh.pipeline_value();

		if(!mesh || path.empty())
			return;

		k3d::log() << info << "STLMeshWriter: Writing .stl file: " << path.native_console_string() << std::endl;

		k3d::filesystem::ofstream file(path);
		if(!file)
		{
			k3d::log() << error << k3d_file_reference << ": error opening [" << path.native_console_string() << "]" << std::endl;
			return;
		}

		file << "solid\n";

		// Store polyhedra ...
		boost::scoped_ptr<k3d::polyhedron::const_primitive> polyhedron(k3d::polyhedron::validate(*mesh));
		if(!polyhedron)
			return;
		
		return_if_fail(k3d::is_triangles(*mesh));

		const k3d::mesh::points_t& points = *(mesh->points);
		
		const k3d::uint_t shell_begin = 0;
		const k3d::uint_t shell_end = shell_begin + polyhedron->first_faces.size();
		for(k3d::uint_t shell = shell_begin; shell != shell_end; ++shell)
		{
			const k3d::uint_t face_begin = polyhedron->first_faces[shell];
			const k3d::uint_t face_end = face_begin + polyhedron->face_counts[shell];
			for(k3d::uint_t face = face_begin; face != face_end; ++face)
			{
				file << "facet normal 0.0 0.0 0.0\n";
				file << "  outer loop\n";
				const k3d::uint_t loop_begin = polyhedron->face_first_loops[face];
				const k3d::uint_t loop_end = loop_begin + polyhedron->face_loop_counts[face];
				for(k3d::uint_t loop = loop_begin; loop != loop_end; ++loop)
				{
					const k3d::uint_t first_edge = polyhedron->loop_first_edges[loop];
					for(k3d::uint_t edge = first_edge; ; )
					{
						file << "    vertex " << points[polyhedron->edge_points[edge]] << "\n";

						edge = polyhedron->clockwise_edges[edge];
						if(edge == first_edge)
							break;
					}
					break;
				}
				file << "  endloop\n";
				file << "endfacet\n";
			}
		}
	}

	static k3d::iplugin_factory& get_factory()
	{
		static k3d::document_plugin_factory<mesh_writer, k3d::interface_list<k3d::imesh_sink > > factory(
			k3d::uuid(0x1de22bd7, 0xee49eca0, 0x827240b3, 0xaf18e521),
			"STLMeshWriter",
			_("Mesh writer that saves external Stereo Triangulation Lithograph (.stl) files"),
			"MeshWriter",
			k3d::iplugin_factory::EXPERIMENTAL);

		return factory;
	}

private:
	k3d_data(k3d::filesystem::path, immutable_name, change_signal, with_undo, local_storage, no_constraint, path_property, path_serialization) m_file;
};

k3d::iplugin_factory& mesh_writer_factory()
{
	return mesh_writer::get_factory();
}

} // namespace io

} // namespace stl

} // namespace module
