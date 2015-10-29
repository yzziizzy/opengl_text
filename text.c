
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)

#include <GL/glew.h>



#include "../c3dlas/c3dlas.h"
#include "text.h"
#include "../EACSMB/src/utilities.h"



FT_Library ftLib = NULL;

static void makeVertices(TextRenderInfo* tri, unsigned int* colors);


/*
A Note About Charsets:

All characters specified will be packed into a texture then shoved into video ram.
There is a limit to the size of textures. A large font size and large character 
set may not fit in the largest texture available. Even if it does, precious vram 
is used for every character added. This is not the place to be a unicode twat;
specify only the characters you will actually use. And for God's sake, don't try
to render traditional Chinese at 64pt... you'd use 80+mb of ram.

Also note the kerning information is n^2 in size with respect to strlen(charset).
Currently this information is in system memory but eventually most operations
will be moved to the gpu. Geometry shaders are amazing.

*/

// all keys on a standard US keyboard.
char* defaultCharset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 `~!@#$%^&*()_+|-=\\{}[]:;<>?,./'\"";

// 16.16 fixed point to float conversion
static float f2f(int i) {
	return (float)(i >> 6);
}


static void blit(
	int src_x, int src_y, int dst_x, int dst_y, int w, int h,
	int src_w, int dst_w, unsigned char* src, unsigned char* dst) {
	
	
	int y, x, s, d;
	
	// this may be upside down...
	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++) {
			s = ((y + src_y) * src_w) + src_x + x;
			d = ((y + dst_y) * dst_w) + dst_x + x;
			
			dst[d] = src[s];
		}
	}
}




TextRes* LoadFont(char* path, int size, char* chars) {

	FT_Error err;
	FT_GlyphSlot slot;
	TextRes* res;
	int i, j, charlen, width, h_above, h_below, height, padding, xoffset;
	
	padding = 2;
	
	if(!ftLib) {
		err = FT_Init_FreeType(&ftLib);
		if(err) {
			fprintf(stderr, "Could not initialize FreeType library.\n");
			return NULL;
		}
	}
	
	res = (TextRes*)malloc(sizeof(TextRes));
	// if you're out of memory you have bigger problems than error checking...
	
	err = FT_New_Face(ftLib, path, 0, &res->fontFace);
	if(err) {
		fprintf(stderr, "Could not load font file \"%s\".\n", path);
		free(res);
		return NULL;
	}
	
	err = FT_Set_Pixel_Sizes(res->fontFace, 0, size);
	if(err) {
		fprintf(stderr, "Could not set pixel size to %dpx.\n", size);
		free(res);
		return NULL;
	}
	
	// slot is a pointer
	slot = res->fontFace->glyph;
	
	if(!chars) chars = defaultCharset;
	res->charSet = strdup(chars);
	
	charlen = strlen(chars);
	res->charLen = charlen;
	
	h_above = 0;
	h_below = 0;
	width = 0;
	height = 0;
	
	// first find how wide of a texture we need
	for(i = 0; i < charlen; i++) { 
		int ymin;
		err = FT_Load_Char(res->fontFace, chars[i], FT_LOAD_DEFAULT);
		
		ymin = slot->metrics.height >> 6;// - f2f(slot->metrics.horiBearingY);
/*
		printf("%c-----\nymin: %d \n", chars[i], ymin);
		printf("bearingY: %d \n", slot->metrics.horiBearingY >> 6);
		printf("width: %d \n", slot->metrics.width >> 6);
		printf("height: %d \n\n", slot->metrics.height >> 6);
*/		
		
		width += f2f(slot->metrics.width);
		h_above = MAX(h_above, slot->metrics.horiBearingY >> 6);
		h_below = MAX(h_below, ymin);
	}
	
	
	width += charlen * padding;
	height = h_below + padding + padding;
	
	res->maxHeight = height;
	res->padding = padding / 2;
	
	//TODO: clean up this messy function
//	printf("width: %d, height: %d \n", width, height);

	width = nextPOT(width);
	height = nextPOT(height);
	res->texWidth = width;
	res->texHeight = height; // may not always just be one row
	
	
	res->texture = (unsigned char*)calloc(width * height, 1);
	res->offsets = (unsigned short*)calloc(charlen * sizeof(unsigned short), 1);
	res->charWidths = (unsigned short*)calloc(charlen * sizeof(unsigned short), 1);
	res->valign = (unsigned char*)calloc(charlen * sizeof(unsigned char), 1);
	

	// construct code mapping
	// shitty method for now, improve later
	res->indexLen = 128; // 7 bits for now.
	res->codeIndex = (unsigned char*)calloc(res->indexLen, 1);
	
	// render the glyphs into the texture
	
	xoffset = 0;
	for(i = 0; i < charlen; i++) { 
		int paddedw, charHeight, bearingY;
		
		
		err = FT_Load_Char(res->fontFace, chars[i], FT_LOAD_RENDER);
		
		paddedw = (slot->metrics.width >> 6) + padding;
		bearingY = slot->metrics.horiBearingY >> 6;
		charHeight = slot->metrics.height >> 6;
		
		res->charWidths[i] = paddedw + padding;
		
/*		printf("meh: %d\n", height - charHeight); 
		printf("index: %d, char: %c, xoffset: %d, pitch: %d \n", i, chars[i], xoffset, slot->bitmap.pitch);
		printf("m.width: %d, m.height: %d, hbearing: %d, habove: %d \n\n", slot->metrics.width >> 6, slot->metrics.height >> 6, slot->metrics.horiBearingY >> 6, h_above);
*/		blit(
			0, 0, // src x and y offset for the image
			xoffset + padding, padding + (h_above - bearingY), // dst offset
			slot->metrics.width >> 6, slot->metrics.height >> 6, // width and height BUG probably
			slot->bitmap.pitch, width, // src and dst row widths
			slot->bitmap.buffer, // source
			res->texture); // destination
		
		
		res->codeIndex[chars[i]] = i;
		res->offsets[i] = xoffset;
		res->valign[i] = (height - h_above + bearingY - padding);// + (slot->metrics.horiBearingY >> 6);
		xoffset += paddedw;
	}
	
	

	
	// kerning map
	res->kerning = (unsigned char*)malloc(charlen * charlen);
	for(i = 0; i < charlen; i++) {
		FT_UInt left, right;
		FT_Vector k;
		
		left = FT_Get_Char_Index(res->fontFace, chars[i]);
		
		for(j = 0; j < charlen; j++) {
			
			right = FT_Get_Char_Index(res->fontFace, chars[j]);
			
			FT_Get_Kerning(res->fontFace, left, right, FT_KERNING_DEFAULT, &k);
		//	if(k.x != 0) printf("k: (%c%c) %d, %d\n", chars[i],chars[j], k.x, k.x >> 6);
			res->kerning[(i * charlen) + j] = k.x >> 6;
		}
	}
	
	
	
	//////////// opengl stuff ////////////
	
	// TODO: error checking
	glGenTextures(1, &res->textureID);
	glBindTexture(GL_TEXTURE_2D, res->textureID);
	glerr("bind font tex");
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); // no mipmaps for this; it'll get fucked up
	glerr("param font tex");
	
	printf("width: %d, height: %d \n", width, height);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, res->texture);
	glerr("load font tex");
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	
	return res;
}




TextRenderInfo* prepareText(TextRes* font, const char* str, int len, unsigned int* colors) {
	
	float offset;
	int v, i;
	TextRenderInfo* tri;
	float uscale, vscale, scale;
	unsigned int color;
	
	unsigned int defaultColor[] = {0xBADA55FF, INT_MAX}; // you have serious problems if the string is longer than INT_MAX
	
	if(!colors)
		colors = &defaultColor;
	
	//TODO: 
	// normalize uv's
	// investigate and fix kerning, it seems off
	// move VAO to a better spot
	
	
	if(len == -1) len = strlen(str);
	
	// create vao/vbo
	tri = (TextRenderInfo*)malloc(sizeof(TextRenderInfo));
	tri->vertices = (TextVertex*)malloc(len * 2 * 3 * sizeof(TextVertex));
	tri->font = font;
	tri->text = strdup(str);
	tri->textLen = len;
	
	//move this to a global
	glGenVertexArrays(1, &tri->vao);
	glBindVertexArray(tri->vao);

	// have to create and bind the buffer before specifying its format
	glGenBuffers(1, &tri->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, tri->vbo);

	
	// vertex
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TextVertex), 0);
	glerr("pos attrib");
	// uvs
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), 3*4);
	glerr("uv attrib");

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextVertex), 5*4);
	glerr("color attrib");

	
	makeVertices(tri, colors);
	
 	glBufferData(GL_ARRAY_BUFFER, tri->vertexCnt * sizeof(TextVertex), tri->vertices, GL_STATIC_DRAW);
//	glBufferData(GL_ARRAY_BUFFER, sizeof(testarr), testarr, GL_STATIC_DRAW);
	glerr("buffering text vertices");
	
	// init shaders elsewhere
	
	return tri;
}


// internal use.
static void makeVertices(TextRenderInfo* tri, unsigned int* colors) {
	float offset, uscale, vscale, scale;
	int v, i;
	char* str;
	TextRes* font;
	unsigned int color;
	
	str = tri->text;
	font = tri->font;
	
	uscale = 1.0 / font->texWidth; 
	vscale = 1.0 / font->texHeight; 
	scale = 1.0 / font->maxHeight;
	
	offset = 0;
	v = 0;
	for(i = 0; i < tri->textLen; i++) {
		float width, valign, kerning;
		float tex_offset, to_next;
		int index, prev;
		
		//increment the colors pointer if we reach the next change point
		if(i == colors[1]) colors += 2;
		
		color = *colors;
		
		
		index = font->codeIndex[str[i]];
		prev = font->codeIndex[str[i-1]];
// 		width = font->kerning[index];
		width = font->charWidths[index] * scale;
		tex_offset = (font->offsets[index] + font->padding) * uscale;
		to_next = (font->offsets[index + 1] + font->padding) * uscale; // bug at end of array
		
		kerning = 0;
		if(i > 0)
			kerning = (font->kerning[(index * font->charLen) + prev]) * uscale; // bug at end of array
		
		offset -= (font->padding * 2) * vscale;
		offset -= kerning;
	/*	
		printf("kerning: %f\n", kerning);
		
		printf("index: %d, char: %c\n", index, str[i]);
		printf("offset %f\n", tex_offset);
		printf("next o %f\n", (float)to_next * uscale);
		printf("uscale %f\n", uscale);
		printf("valign %d\n", font->valign[index]);
		printf("width %f\n\n", width);
	*/
		//tex_offset = 1;

		// add quad, set uv's
		// triangle 1
		tri->vertices[v].x = width + offset; 
		tri->vertices[v].y = 1.0 + valign;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].rgba = color;
		tri->vertices[v].u = to_next;
		tri->vertices[v++].v = 1.0;
		
		// top left
		tri->vertices[v].x = width + offset; 
		tri->vertices[v].y = 0.0 + valign;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].rgba = color;
		tri->vertices[v].u = to_next;
		tri->vertices[v++].v = 0.0;

		// top right
		tri->vertices[v].x = 0.0 + offset; 
		tri->vertices[v].y = 1.0 + valign;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].rgba = color;
		tri->vertices[v].u = tex_offset;
		tri->vertices[v++].v = 1.0;
		
		
		// triangle 2
		tri->vertices[v].x = 0.0 + offset; 
		tri->vertices[v].y = 1.0 + valign;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].rgba = color;
		tri->vertices[v].u = tex_offset;
		tri->vertices[v++].v = 1.0;
		
		// top right
		tri->vertices[v].x = width + offset; 
		tri->vertices[v].y = 0.0 + valign;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].rgba = color;
		tri->vertices[v].u = to_next;
		tri->vertices[v++].v = 0.0;
		
		// bottom right
		tri->vertices[v].x = 0.0 + offset; 
		tri->vertices[v].y = 0.0 + valign;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].rgba = color;
		tri->vertices[v].u = tex_offset;
		tri->vertices[v++].v = 0.0;
		
		
		// move right by kerning amount
		offset += width; //tex_offset;
	}
	
	tri->vertexCnt = v;

}


void FreeFont(TextRes* res) {
	free(res->texture);
	free(res->codeIndex);
	free(res->offsets);
	free(res->valign);
	free(res->kerning);
	free(res->charSet);
	
	free(res);
};
	
	
// super nifty site:
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
int nextPOT(int in) {
	
	in--;
	in |= in >> 1;
	in |= in >> 2;
	in |= in >> 4;
	in |= in >> 8;
	in |= in >> 16;
	in++;
	
	return in;
}


void updateText(TextRenderInfo* tri, const char* str, int len, unsigned int* colors) {
	
	if(len <= 0) len = strlen(str);
	
	// text hasn't changed
	if(0 == strcmp(str, tri->text)) return;
	
	// need to check if colors changes too, but meh
	
	// reallocate the vertex data buffer if it's too small
	if(tri->vertices && len > tri->textLen) {
		free(tri->vertices);
		tri->vertices = (TextVertex*)malloc(len * 2 * 3 * sizeof(TextVertex));
	}
	
	if(tri->text) free(tri->text);
	tri->text = strdup(str);
	tri->textLen = len;
	
	// i've read that trying to update a vbo when opengl is rendering can force a
	// disasterous gpu<->cpu sync. here the old vbo is released (hopefully async)
	// and the next frame will get the new vbo (all hopefully async). this has not 
	// been comparatively tested.
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	GLuint oldvbo = tri->vbo;
	
	glBindVertexArray(tri->vao);
	
	glGenBuffers(1, &tri->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, tri->vbo);
	glexit("update text buffer creation");
	
	glDeleteBuffers(1, &oldvbo);
	
		// vertex
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TextVertex), 0);
	glerr("pos attrib");
	// uvs
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), 3*4);
	glerr("uv attrib");

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextVertex), 5*4);
	glerr("color attrib");


	
	makeVertices(tri, colors);
	
 	glBufferData(GL_ARRAY_BUFFER, tri->vertexCnt * sizeof(TextVertex), tri->vertices, GL_STATIC_DRAW);
//	glBufferData(GL_ARRAY_BUFFER, sizeof(testarr), testarr, GL_STATIC_DRAW);
	glerr("buffering text vertices");

}


