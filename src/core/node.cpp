#include "node.hpp"
#include "helpers.hpp"

#include "core/Log.h"
#include "core/opengl.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void
Node::render(glm::mat4 const& view_projection, glm::mat4 const& parent_transform) const
{
	if (_program != nullptr)
		render(view_projection, parent_transform * _transform.GetMatrix(), *_program, _set_uniforms);
}
void
Node::render(glm::mat4 const& view_projection, int instanceNum, std::vector<glm::vec2>& positions, std::vector<glm::vec2>& velocities, glm::mat4 const& parent_transform) const
{
	if (_program != nullptr)
		render(view_projection, parent_transform * _transform.GetMatrix(), instanceNum, positions, velocities, *_program, _set_uniforms);
}
void
Node::render(glm::mat4 const& view_projection, int instanceNum, std::vector<glm::vec3>& positions, std::vector<glm::vec3>& velocities, glm::mat4 const& parent_transform) const
{
	if (_program != nullptr)
		render(view_projection, parent_transform * _transform.GetMatrix(), instanceNum, positions, velocities, *_program, _set_uniforms);
}
void
Node::render(glm::mat4 const& view_projection,glm::vec2 position ,glm::mat4 const& parent_transform) const
{
	glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, 0.0f));
	glm::mat4 resultMatrix = translationMatrix * _transform.GetMatrix();
	if (_program != nullptr)
		render(view_projection, parent_transform * resultMatrix, *_program, _set_uniforms);
}
void
Node::render(glm::mat4 const& view_projection, int instanceNum, GLuint positionsBuffer, GLuint velocitiesBuffer, glm::mat4 const& parent_transform) const
{
	if (_program != nullptr)
		render(view_projection, parent_transform * _transform.GetMatrix(), instanceNum, positionsBuffer, velocitiesBuffer, *_program, _set_uniforms);
}

void
Node::render(glm::mat4 const& view_projection, glm::mat4 const& world, GLuint program, std::function<void (GLuint)> const& set_uniforms) const
{
	if (_vao == 0u || program == 0u)
		return;

	utils::opengl::debug::beginDebugGroup(_name);

	glUseProgram(program);

	auto const normal_model_to_world = glm::transpose(glm::inverse(world));

	set_uniforms(program);

	glUniformMatrix4fv(glGetUniformLocation(program, "vertex_model_to_world"), 1, GL_FALSE, glm::value_ptr(world));
	glUniformMatrix4fv(glGetUniformLocation(program, "normal_model_to_world"), 1, GL_FALSE, glm::value_ptr(normal_model_to_world));
	glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_clip"), 1, GL_FALSE, glm::value_ptr(view_projection));

	for (size_t i = 0u; i < _textures.size(); ++i) {
		auto const& texture = _textures[i];
		glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
		glBindTexture(std::get<2>(texture), std::get<1>(texture));
		glUniform1i(glGetUniformLocation(program, std::get<0>(texture).c_str()), static_cast<GLint>(i));

		std::string texture_presence_var_name = "has_" + std::get<0>(texture);
		glUniform1i(glGetUniformLocation(program, texture_presence_var_name.c_str()), 1);
	}

	glUniform3fv(glGetUniformLocation(program, "diffuse_colour"), 1, glm::value_ptr(_constants.diffuse));
	glUniform3fv(glGetUniformLocation(program, "specular_colour"), 1, glm::value_ptr(_constants.specular));
	glUniform3fv(glGetUniformLocation(program, "ambient_colour"), 1, glm::value_ptr(_constants.ambient));
	glUniform3fv(glGetUniformLocation(program, "emissive_colour"), 1, glm::value_ptr(_constants.emissive));
	glUniform1f(glGetUniformLocation(program, "shininess_value"), _constants.shininess);
	glUniform1f(glGetUniformLocation(program, "index_of_refraction_value"), _constants.indexOfRefraction);
	glUniform1f(glGetUniformLocation(program, "opacity_value"), _constants.opacity);

	glBindVertexArray(_vao);
	if (_has_indices)
		glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
	else
		glDrawArrays(_drawing_mode, 0, _vertices_nb);
	glBindVertexArray(0u);

	for (auto const& texture : _textures) {
		glBindTexture(std::get<2>(texture), 0);
		glUniform1i(glGetUniformLocation(program, std::get<0>(texture).c_str()), 0);

		std::string texture_presence_var_name = "has_" + std::get<0>(texture);
		glUniform1i(glGetUniformLocation(program, texture_presence_var_name.c_str()), 0);
	}

	glUseProgram(0u);

	utils::opengl::debug::endDebugGroup();
}
void
Node::render(glm::mat4 const& view_projection, glm::mat4 const& world, int instanceNum, std::vector<glm::vec2>& positions, std::vector<glm::vec2>& velocities, GLuint program, std::function<void(GLuint)> const& set_uniforms) const
{
	if (_vao == 0u || program == 0u)
		return;

	utils::opengl::debug::beginDebugGroup(_name);

	glUseProgram(program);

	auto const normal_model_to_world = glm::transpose(glm::inverse(world));

	set_uniforms(program);

	glUniformMatrix4fv(glGetUniformLocation(program, "vertex_model_to_world"), 1, GL_FALSE, glm::value_ptr(world));
	glUniformMatrix4fv(glGetUniformLocation(program, "normal_model_to_world"), 1, GL_FALSE, glm::value_ptr(normal_model_to_world));
	glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_clip"), 1, GL_FALSE, glm::value_ptr(view_projection));

	for (size_t i = 0u; i < _textures.size(); ++i) {
		auto const& texture = _textures[i];
		glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
		glBindTexture(std::get<2>(texture), std::get<1>(texture));
		glUniform1i(glGetUniformLocation(program, std::get<0>(texture).c_str()), static_cast<GLint>(i));

		std::string texture_presence_var_name = "has_" + std::get<0>(texture);
		glUniform1i(glGetUniformLocation(program, texture_presence_var_name.c_str()), 1);
	}

	glUniform3fv(glGetUniformLocation(program, "diffuse_colour"), 1, glm::value_ptr(_constants.diffuse));
	glUniform3fv(glGetUniformLocation(program, "specular_colour"), 1, glm::value_ptr(_constants.specular));
	glUniform3fv(glGetUniformLocation(program, "ambient_colour"), 1, glm::value_ptr(_constants.ambient));
	glUniform3fv(glGetUniformLocation(program, "emissive_colour"), 1, glm::value_ptr(_constants.emissive));
	glUniform1f(glGetUniformLocation(program, "shininess_value"), _constants.shininess);
	glUniform1f(glGetUniformLocation(program, "index_of_refraction_value"), _constants.indexOfRefraction);
	glUniform1f(glGetUniformLocation(program, "opacity_value"), _constants.opacity);

	glBindBuffer(GL_ARRAY_BUFFER, _vao);
	//buffer map
	GLfloat* mappedBufferPosition = static_cast<GLfloat*>(glMapBufferRange(GL_ARRAY_BUFFER, _vertices_nb * sizeof(glm::vec3), 2 * positions.size() * sizeof(glm::vec2), GL_MAP_WRITE_BIT));
	auto velovityOffset = 2 * positions.size();
	if (mappedBufferPosition) {
		// updata
		for (int i = 0; i < positions.size(); i++) {
			mappedBufferPosition[2 * i] = positions[i].x;
			mappedBufferPosition[2 * i + 1] = positions[i].y;
			mappedBufferPosition[velovityOffset + 2 * i] = velocities[i].x;
			mappedBufferPosition[velovityOffset + 2 * i + 1] = velocities[i].y;
		}
		// cancel mapping
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
	//std::cout << _vertices_nb << std::endl;
	//std::cout << instanceNum << std::endl;
	glBindVertexArray(_vao);
	if (_has_indices) {
		if (instanceNum == 1) {
			glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
		}
		else {
			glDrawElementsInstanced(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0), instanceNum);
			//std::cout << "instance render" << std::endl;
		}
	}
	else {
		if (instanceNum == 1) {
			glDrawArrays(_drawing_mode, 0, _vertices_nb);
		}
		else {
			glDrawArraysInstanced(_drawing_mode, 0, _vertices_nb, instanceNum);
		}
	}
	glBindVertexArray(0u);

	//glBindVertexArray(_vao);
	//if (_has_indices)
	//	glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
	//else
	//	glDrawArrays(_drawing_mode, 0, _vertices_nb);
	//glBindVertexArray(0u);

	for (auto const& texture : _textures) {
		glBindTexture(std::get<2>(texture), 0);
		glUniform1i(glGetUniformLocation(program, std::get<0>(texture).c_str()), 0);

		std::string texture_presence_var_name = "has_" + std::get<0>(texture);
		glUniform1i(glGetUniformLocation(program, texture_presence_var_name.c_str()), 0);
	}

	glUseProgram(0u);

	utils::opengl::debug::endDebugGroup();
}
void
Node::render(glm::mat4 const& view_projection, glm::mat4 const& world, int instanceNum, std::vector<glm::vec3>& positions, std::vector<glm::vec3>& velocities, GLuint program, std::function<void(GLuint)> const& set_uniforms) const
{
	if (_vao == 0u || program == 0u)
		return;

	utils::opengl::debug::beginDebugGroup(_name);

	glUseProgram(program);

	auto const normal_model_to_world = glm::transpose(glm::inverse(world));

	set_uniforms(program);

	glUniformMatrix4fv(glGetUniformLocation(program, "vertex_model_to_world"), 1, GL_FALSE, glm::value_ptr(world));
	glUniformMatrix4fv(glGetUniformLocation(program, "normal_model_to_world"), 1, GL_FALSE, glm::value_ptr(normal_model_to_world));
	glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_clip"), 1, GL_FALSE, glm::value_ptr(view_projection));

	for (size_t i = 0u; i < _textures.size(); ++i) {
		auto const& texture = _textures[i];
		glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
		glBindTexture(std::get<2>(texture), std::get<1>(texture));
		glUniform1i(glGetUniformLocation(program, std::get<0>(texture).c_str()), static_cast<GLint>(i));

		std::string texture_presence_var_name = "has_" + std::get<0>(texture);
		glUniform1i(glGetUniformLocation(program, texture_presence_var_name.c_str()), 1);
	}

	glUniform3fv(glGetUniformLocation(program, "diffuse_colour"), 1, glm::value_ptr(_constants.diffuse));
	glUniform3fv(glGetUniformLocation(program, "specular_colour"), 1, glm::value_ptr(_constants.specular));
	glUniform3fv(glGetUniformLocation(program, "ambient_colour"), 1, glm::value_ptr(_constants.ambient));
	glUniform3fv(glGetUniformLocation(program, "emissive_colour"), 1, glm::value_ptr(_constants.emissive));
	glUniform1f(glGetUniformLocation(program, "shininess_value"), _constants.shininess);
	glUniform1f(glGetUniformLocation(program, "index_of_refraction_value"), _constants.indexOfRefraction);
	glUniform1f(glGetUniformLocation(program, "opacity_value"), _constants.opacity);

	glBindBuffer(GL_ARRAY_BUFFER, _vao);
	//buffer map
	GLfloat* mappedBufferPosition = static_cast<GLfloat*>(glMapBufferRange(GL_ARRAY_BUFFER, _vertices_nb * sizeof(glm::vec3), 2 * positions.size() * sizeof(glm::vec3), GL_MAP_WRITE_BIT));
	auto velovityOffset = 3 * positions.size();
	if (mappedBufferPosition) {
		// updata
		for (int i = 0; i < positions.size(); i++) {
			mappedBufferPosition[3 * i] = positions[i].x;
			mappedBufferPosition[3 * i + 1] = positions[i].y;
			mappedBufferPosition[3 * i + 2] = positions[i].z;
			mappedBufferPosition[velovityOffset + 3 * i] = velocities[i].x;
			mappedBufferPosition[velovityOffset + 3 * i + 1] = velocities[i].y;
			mappedBufferPosition[velovityOffset + 3 * i + 2] = velocities[i].z;
		}
		// cancel mapping
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
	//std::cout << _vertices_nb << std::endl;
	//std::cout << instanceNum << std::endl;
	glBindVertexArray(_vao);
	if (_has_indices) {
		if (instanceNum == 1) {
			glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
		}
		else {
			glDrawElementsInstanced(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0), instanceNum);
			//std::cout << "2instance render" << std::endl;
		}
	}
	else {
		if (instanceNum == 1) {
			glDrawArrays(_drawing_mode, 0, _vertices_nb);
		}
		else {
			glDrawArraysInstanced(_drawing_mode, 0, _vertices_nb, instanceNum);
		}
	}
	glBindVertexArray(0u);

	//glBindVertexArray(_vao);
	//if (_has_indices)
	//	glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
	//else
	//	glDrawArrays(_drawing_mode, 0, _vertices_nb);
	//glBindVertexArray(0u);

	for (auto const& texture : _textures) {
		glBindTexture(std::get<2>(texture), 0);
		glUniform1i(glGetUniformLocation(program, std::get<0>(texture).c_str()), 0);

		std::string texture_presence_var_name = "has_" + std::get<0>(texture);
		glUniform1i(glGetUniformLocation(program, texture_presence_var_name.c_str()), 0);
	}

	glUseProgram(0u);

	utils::opengl::debug::endDebugGroup();
}
void
Node::render(glm::mat4 const& view_projection, glm::mat4 const& world, int instanceNum, GLuint positions, GLuint velocities, GLuint program, std::function<void(GLuint)> const& set_uniforms) const
{
	if (_vao == 0u || program == 0u)
		return;

	utils::opengl::debug::beginDebugGroup(_name);

	glUseProgram(program);

	auto const normal_model_to_world = glm::transpose(glm::inverse(world));

	set_uniforms(program);

	glUniformMatrix4fv(glGetUniformLocation(program, "vertex_model_to_world"), 1, GL_FALSE, glm::value_ptr(world));
	glUniformMatrix4fv(glGetUniformLocation(program, "normal_model_to_world"), 1, GL_FALSE, glm::value_ptr(normal_model_to_world));
	glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_clip"), 1, GL_FALSE, glm::value_ptr(view_projection));

	for (size_t i = 0u; i < _textures.size(); ++i) {
		auto const& texture = _textures[i];
		glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
		glBindTexture(std::get<2>(texture), std::get<1>(texture));
		glUniform1i(glGetUniformLocation(program, std::get<0>(texture).c_str()), static_cast<GLint>(i));

		std::string texture_presence_var_name = "has_" + std::get<0>(texture);
		glUniform1i(glGetUniformLocation(program, texture_presence_var_name.c_str()), 1);
	}

	glUniform3fv(glGetUniformLocation(program, "diffuse_colour"), 1, glm::value_ptr(_constants.diffuse));
	glUniform3fv(glGetUniformLocation(program, "specular_colour"), 1, glm::value_ptr(_constants.specular));
	glUniform3fv(glGetUniformLocation(program, "ambient_colour"), 1, glm::value_ptr(_constants.ambient));
	glUniform3fv(glGetUniformLocation(program, "emissive_colour"), 1, glm::value_ptr(_constants.emissive));
	glUniform1f(glGetUniformLocation(program, "shininess_value"), _constants.shininess);
	glUniform1f(glGetUniformLocation(program, "index_of_refraction_value"), _constants.indexOfRefraction);
	glUniform1f(glGetUniformLocation(program, "opacity_value"), _constants.opacity);

	glBindBuffer(GL_ARRAY_BUFFER, _vao);
	//buffer map
	//GLfloat* mappedBufferPosition = static_cast<GLfloat*>(glMapBufferRange(GL_ARRAY_BUFFER, _vertices_nb * sizeof(glm::vec3), 2 * positions.size() * sizeof(glm::vec2), GL_MAP_WRITE_BIT));
	//auto velovityOffset = 2 * positions.size();
	//if (mappedBufferPosition) {
	//	// updata
	//	for (int i = 0; i < positions.size(); i++) {
	//		mappedBufferPosition[2 * i] = positions[i].x;
	//		mappedBufferPosition[2 * i + 1] = positions[i].y;
	//		mappedBufferPosition[velovityOffset + 2 * i] = velocities[i].x;
	//		mappedBufferPosition[velovityOffset + 2 * i + 1] = velocities[i].y;
	//	}
	//	// cancel mapping
	//	glUnmapBuffer(GL_ARRAY_BUFFER);
	//}
	//std::cout << _vertices_nb << std::endl;
	//std::cout << instanceNum << std::endl;
	glBindVertexArray(_vao);
	if (_has_indices) {
		if (instanceNum == 1) {
			glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
		}
		else {
			glDrawElementsInstanced(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0), instanceNum);
			//std::cout << "instance render" << std::endl;
		}
	}
	else {
		if (instanceNum == 1) {
			glDrawArrays(_drawing_mode, 0, _vertices_nb);
		}
		else {
			glDrawArraysInstanced(_drawing_mode, 0, _vertices_nb, instanceNum);
		}
	}
	glBindVertexArray(0u);

	//glBindVertexArray(_vao);
	//if (_has_indices)
	//	glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT, reinterpret_cast<GLvoid const*>(0x0));
	//else
	//	glDrawArrays(_drawing_mode, 0, _vertices_nb);
	//glBindVertexArray(0u);

	for (auto const& texture : _textures) {
		glBindTexture(std::get<2>(texture), 0);
		glUniform1i(glGetUniformLocation(program, std::get<0>(texture).c_str()), 0);

		std::string texture_presence_var_name = "has_" + std::get<0>(texture);
		glUniform1i(glGetUniformLocation(program, texture_presence_var_name.c_str()), 0);
	}

	glUseProgram(0u);

	utils::opengl::debug::endDebugGroup();
}


void
Node::set_geometry(bonobo::mesh_data const& shape)
{
	_vao = shape.vao;
	_vertices_nb = static_cast<GLsizei>(shape.vertices_nb);
	_indices_nb = static_cast<GLsizei>(shape.indices_nb);
	_drawing_mode = shape.drawing_mode;
	_has_indices = shape.ibo != 0u;
	_name = std::string("Render ") + shape.name;

	if (!shape.bindings.empty()) {
		for (auto const& binding : shape.bindings)
			add_texture(binding.first, binding.second, GL_TEXTURE_2D);
	}

	_constants = shape.material;
}

void
Node::set_material_constants(bonobo::material_data const& constants)
{
	_constants = constants;
}

void
Node::set_program(GLuint const* const program, std::function<void (GLuint)> const& set_uniforms)
{
	if (program == nullptr) {
		LogError("Program can not be a null pointer; this operation will be discarded.");
		return;
	}

	_program = program;
	_set_uniforms = set_uniforms;
}

void
Node::set_name(std::string const& name)
{
	_name = std::string("Render ") + name;
}

size_t
Node::get_indices_nb() const
{
	return static_cast<size_t>(_indices_nb);
}

void
Node::set_indices_nb(size_t const& indices_nb)
{
	_indices_nb = static_cast<GLsizei>(indices_nb);
}

void
Node::add_texture(std::string const& name, GLuint tex_id, GLenum type)
{
	GLint max_combined_texture_image_units{-1};
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_combined_texture_image_units);
	std::size_t const max_active_texture_count
		= (max_combined_texture_image_units > 0) ? static_cast<std::size_t>(max_combined_texture_image_units)
		                                         : 80; // OpenGL 4.x guarantees at least 80.

	if (_textures.size() >= max_active_texture_count) {
		LogWarning("Trying to add more textures to an object than supported (%llu); the texture %s with ID %u will **not** be added. If you really need that many textures, do not use the `Node` class and roll your own solution instead.",
		           max_active_texture_count, name.c_str(), tex_id);
		return;
	}
	if (tex_id == 0u) {
		LogWarning("0 is not a valid texture ID; the texture %s (with ID %u) will **not** be added.",
		           name.c_str(), tex_id);
		return;
	}

	_textures.emplace_back(name, tex_id, type);
}

void
Node::add_child(Node const* child)
{
	if (child == nullptr) {
		LogWarning("Trying to add a null pointer as child: this will be discarded.");
		return;
	}

	_children.emplace_back(child);
}

size_t
Node::get_children_nb() const
{
	return _children.size();
}

Node const*
Node::get_child(size_t index) const
{
	assert(index < _children.size());
	return _children[index];
}

TRSTransformf const&
Node::get_transform() const
{
	return _transform;
}

TRSTransformf&
Node::get_transform()
{
	return _transform;
}
