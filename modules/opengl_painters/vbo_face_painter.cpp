// K-3D
// Copyright (c) 1995-2005, Timothy M. Shead
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

/** \file Painter that triangulates its input first
 * 	\author Bart Janssens (bart.janssens@lid.kviv.be)
 */

#include <k3dsdk/array.h>
#include <k3dsdk/document_plugin_factory.h>
#include <k3dsdk/extension_gl.h>
#include <k3dsdk/gl.h>
#include <k3dsdk/hints.h>
#include <k3d-i18n-config.h>
#include <k3dsdk/imesh_painter_gl.h>
#include <k3dsdk/mesh_operations.h>
#include <k3dsdk/node.h>
#include <k3dsdk/painter_render_state_gl.h>
#include <k3dsdk/painter_selection_state_gl.h>
#include <k3dsdk/persistent.h>
#include <k3dsdk/selection.h>
#include <k3dsdk/utility_gl.h>

#include "cached_triangulation.h"
#include "colored_selection_painter_gl.h"
#include "vbo.h"

namespace module
{

namespace opengl
{

namespace painters
{

/////////////////////////////////////////////////////////////////////////////
// vbo_face_painter

class vbo_face_painter :
	public colored_selection_painter
{
	typedef colored_selection_painter base;

public:
	vbo_face_painter(k3d::iplugin_factory& Factory, k3d::idocument& Document) :
		base(Factory, Document, k3d::color(0.2,0.2,0.2), k3d::color(0.6,0.6,0.6))
	{
	}

	void on_paint_mesh(const k3d::mesh& Mesh, const k3d::gl::painter_render_state& RenderState)
	{
		return_if_fail(k3d::gl::extension::query_vbo());

		if(!validate_polyhedra(Mesh))
			return;
			
		if (k3d::is_sds(Mesh))
			return;

		k3d::gl::store_attributes attributes;

		glFrontFace(RenderState.inside_out ? GL_CCW : GL_CW);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		k3d::gl::set(GL_CULL_FACE, RenderState.draw_two_sided);

		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0, 1.0);
		
		enable_blending();
		
		const color_t color = RenderState.node_selection ? selected_mesh_color() : unselected_mesh_color(RenderState.parent_selection);
		const color_t selected_color = RenderState.show_component_selection ? selected_component_color() : color;
		
		triangle_vbo& vbos = get_data<triangle_vbo>(&Mesh, this);
		face_selection& selected_faces = get_data<face_selection>(&Mesh, this);
		
		size_t face_count = Mesh.polyhedra->face_first_loops->size();
		const selection_records_t& face_selection_records = selected_faces.records();
		
		glEnable(GL_LIGHTING);
		
		clean_vbo_state();
		
		vbos.bind();
		
		if (!face_selection_records.empty())
		{
			for (selection_records_t::const_iterator record = face_selection_records.begin(); record != face_selection_records.end() && record->begin < face_count; ++record)
			{ // color by selection
				const color_t& face_color = record->weight ? selected_color : color;
				k3d::gl::material(GL_FRONT_AND_BACK, GL_DIFFUSE, k3d::color(face_color.red, face_color.green, face_color.blue), face_color.alpha);
				size_t start = record->begin;
				size_t end = record->end;
				end = end > face_count ? face_count : end;
				vbos.draw_range(start, end, this);
			}
		}
		else
		{ // empty selection, everything has the same color
			color4d(color);
			vbos.draw_range(0, face_count, this);
		}
		
		clean_vbo_state();
		disable_blending();
	}
	
	void on_select_mesh(const k3d::mesh& Mesh, const k3d::gl::painter_render_state& RenderState, const k3d::gl::painter_selection_state& SelectionState)
	{
		return_if_fail(k3d::gl::extension::query_vbo());

		if(!validate_polyhedra(Mesh))
			return;
			
		if (k3d::is_sds(Mesh))
			return;
			
		if (!SelectionState.select_faces)
			return;

		k3d::gl::store_attributes attributes;
		
		glDisable(GL_LIGHTING);

		glFrontFace(RenderState.inside_out ? GL_CCW : GL_CW);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		k3d::gl::set(GL_CULL_FACE, RenderState.draw_two_sided);

		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0, 1.0);
		
		triangle_vbo& vbos = get_data<triangle_vbo>(&Mesh, this);
		
		clean_vbo_state();
		
		vbos.bind();
		
		size_t face_count = Mesh.polyhedra->face_first_loops->size();
		for(size_t face = 0; face != face_count; ++face)
		{
			k3d::gl::push_selection_token(k3d::selection::ABSOLUTE_FACE, face);

			vbos.draw_range(face, face+1, this);

			k3d::gl::pop_selection_token(); // ABSOLUTE_FACE
		}
		
		clean_vbo_state();
	}
	
	void on_mesh_changed(const k3d::mesh& Mesh, k3d::iunknown* Hint)
	{
		return_if_fail(k3d::gl::extension::query_vbo());
		
		if(!k3d::validate_polyhedra(Mesh))
			return;
		
		if (k3d::is_sds(Mesh))
			return;
		
		schedule_data<triangle_vbo>(&Mesh, Hint, this);
		schedule_data<face_selection>(&Mesh, Hint, this);
	}
	
	static k3d::iplugin_factory& get_factory()
	{
		static k3d::document_plugin_factory<vbo_face_painter, k3d::interface_list<k3d::gl::imesh_painter > > factory(
			k3d::uuid(0x2f953308, 0xc8474bc9, 0x6d58bba7, 0x0355bcfe),
			"OpenGLVBOFacePainter",
			_("Renders mesh faces, after trianglulating them (OpenGL VBOs)"),
			"Development",
			k3d::iplugin_factory::EXPERIMENTAL);

		return factory;
	}
};

/////////////////////////////////////////////////////////////////////////////
// vbo_face_painter_factory

k3d::iplugin_factory& vbo_face_painter_factory()
{
	return vbo_face_painter::get_factory();
}

} // namespace painters

} // namespace opengl

} // namespace module