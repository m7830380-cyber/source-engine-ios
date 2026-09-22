//========= Source Engine iOS modernization ==============================//
//
// Purpose: Reconcile vertex/fragment varyings before linking.
//
//	THE BUG
//	-------
//	On device the log is full of hard link failures like:
//
//	    shader 2480 link log: FRAGMENT varying oT6 does not match any
//	                          VERTEX varying
//
//	This is NOT a warning. CGLMShaderPair::SetProgramPair only prints the
//	info log when GL_LINK_STATUS is GL_FALSE, so every one of those lines
//	is a program that failed to link. A failed program draws nothing or
//	garbage, which is a direct cause of broken/■■ rendering.
//
//	WHY IT HAPPENS
//	--------------
//	Source pairs vertex and pixel shaders dynamically at draw time - any
//	vs can be matched with any ps. Under D3D9 that was legal: a pixel
//	shader reading an interpolator the vertex shader never wrote just got
//	undefined data, and the runtime allowed it.
//
//	GLSL ES 3.00 is stricter. Section 4.3.10 of the spec makes it a link
//	error for a fragment shader input to have no matching vertex output.
//	So the exact same shader pair that worked under D3D9 now fails.
//
//	Concrete example from the device log:
//	    vs lightmappedgeneric_vs20   declares out oT0, oT1, oT2, oT3, oT4
//	    ps vertexlit_and_unlit_generic_ps20b reads in  oT0, oT1, oT2, oT6
//	                                                             ^^^^
//	oT6 is never written by that vertex shader -> link fails.
//
//	THE FIX
//	-------
//	Before linking, compare the two shaders' varying sets. For every
//	fragment input with no matching vertex output, inject a stub
//	declaration plus an assignment of vec4(0) into the vertex shader, then
//	recompile it. That restores the D3D9 behaviour (reads yield a defined
//	constant instead of garbage) while satisfying the GLSL ES linker.
//
//	This runs only when a mismatch is actually detected, so shader pairs
//	that already agree are untouched and pay nothing.
//
//=======================================================================//

#ifndef GLM_VARYING_FIXUP_H
#define GLM_VARYING_FIXUP_H
#pragma once

// Scan vertex and fragment GLSL for oTn varyings. If the fragment shader
// reads any that the vertex shader does not write, return a patched copy
// of the vertex source with stub outputs added.
//
// The text pointers are NOT assumed to be NUL-terminated: CGLMProgram
// stores each language's source as an offset+length slice inside one
// buffer (see m_textOffset / m_textLength), and Compile() passes the
// length explicitly to glShaderSource for exactly that reason. Lengths
// are therefore required here too.
//
// Returns NULL when no patch is needed. The caller owns the returned
// buffer (which IS NUL-terminated) and must free() it.
char *GLMFixupMissingVaryings( const char *pVertexText, int nVertexLen,
							   const char *pFragmentText, int nFragmentLen );

#endif // GLM_VARYING_FIXUP_H
