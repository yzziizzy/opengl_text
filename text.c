
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)

#include <GL/glew.h>



#include "../c3dlas/c3dlas.h"
#include "text.h"




FT_Library ftLib = NULL;

// all keys on a standard US keyboard.
char* defaultCharset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`~!@#$%^&*()_+|-=\\{}[]:;<>?,./'\"";



void blit(
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
		ymin = slot->metrics.height - slot->metrics.horiBearingY;
		
		err = FT_Load_Char(res->fontFace, chars[i], FT_LOAD_DEFAULT);
		
		width += slot->metrics.width;
		h_above = MAX(h_above, slot->metrics.horiBearingY);
		h_below = MAX(h_below, ymin);
	}
	
	
	width += charlen * padding;
	height = h_above + h_below;
	
	//TODO: clean up this messy function
	
	width = nextPOT(width);
	height = nextPOT(height);
	res->texWidth = width;
	res->texHeight = height;
	
	
	res->texture = (unsigned char*)malloc(width * height);
	res->offsets = (unsigned short*)calloc(charlen * sizeof(unsigned short), 1);
	
	// render the glyphs into the texture
	
	xoffset = 0;
	for(i = 0; i < charlen; i++) { 
		
		err = FT_Load_Char(res->fontFace, chars[i], FT_LOAD_RENDER);
		
		blit(
			0, 0, // src x and y offset for the image
			xoffset, 0, // dst offset
			slot->metrics.width, slot->metrics.height, // width and height BUG probably
			slot->bitmap.pitch, width, // src and dst row widths
			slot->bitmap.buffer, // source
			res->texture); // destination
		
		res->offsets[i] = xoffset;
		xoffset += slot->metrics.width + padding;
	}
	
	// construct code mapping
	// shitty method for now, improve later
	res->indexLen = 128; // 7 bits for now.
	res->texture = (unsigned char*)calloc(res->indexLen, 1);
	
	for(i = 0; i < charlen; i++) 
		res->codeIndex[chars[i]] = i;
	
	
	// kerning map
	res->kerning = (unsigned char*)malloc(charlen * charlen);
	for(i = 0; i < charlen; i++) {
		for(j = 0; j < charlen; j++) {
			
			// TODO: calculate real kerning 
			res->kerning[(i * charlen) + j] = 0;
			
		}
	}
	
	
	//////////// opengl stuff ////////////
	
	// TODO: error checking
	glGenTextures(1, &res->textureID);
	glBindTexture(GL_TEXTURE_2D, res->textureID);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); // no mipmaps for this; it'll get fucked up
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, res->texture);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	
	return res;
}

void prepareText(TextRes* font, const char* str, int len, Matrix* m) {
	
	int offset, v, i;
	TextRenderInfo* tri;
	float uscale, vscale;
	
	
	//TODO: 
	// normalize uv's
	// use real kerning info after it's filled in
	// move VAO to a global
	
	
	if(len == -1) len = strlen(str);
	uscale = 1.0 / font->texWidth; 
	vscale = 1.0 / font->texHeight; 
	
	// create vao/vbo
	tri = (TextRenderInfo*)malloc(sizeof(TextRenderInfo));
	tri->vertices = (TextVertex*)malloc(len * 2 * 3 * sizeof(TextVertex));
	tri->font = font;
	
	//move this to a global
	glGenVertexArrays(1, &tri->vao);
	glBindVertexArray(tri->vao);
	
	// vertex
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	// uvs
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

	
	offset = 0;
	v = 0;
	for(i = 0; i < len; i++) {
		int width;
		int tex_offset, to_next;
		int index;
		
		index = font->codeIndex[str[i]];
// 		width = font->kerning[index];
		width = font->offsets[index];
		tex_offset = font->offsets[index];
		tex_offset = font->offsets[index + 1];
		
		// add quad, set uv's
		// bottom left, triangle 1
		tri->vertices[v].x = 0.0; 
		tri->vertices[v].y = 0.0;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].u = tex_offset * uscale;
		tri->vertices[v++].v = 0.0;
		
		// top left
		tri->vertices[v].x = 0.0; 
		tri->vertices[v].y = 1.0;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].u = tex_offset * uscale;
		tri->vertices[v++].v = 1.0;

		// top right
		tri->vertices[v].x = 0.0; 
		tri->vertices[v].y = 1.0;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].u = to_next * uscale;
		tri->vertices[v++].v = 1.0;
		
		// bottom left, triangle 2
		tri->vertices[v].x = 0.0; 
		tri->vertices[v].y = 0.0;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].u = tex_offset * uscale;
		tri->vertices[v++].v = 0.0;
		
		// top right
		tri->vertices[v].x = 0.0; 
		tri->vertices[v].y = 1.0;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].u = to_next * uscale;
		tri->vertices[v++].v = 1.0;
		
		// bottom right
		tri->vertices[v].x = 1.0; 
		tri->vertices[v].y = 0.0;
		tri->vertices[v].z = 0.0;
		tri->vertices[v].u = to_next * uscale;
		tri->vertices[v++].v = 0.0;
		
		// move right by kerning amount
		offset += tex_offset;
	}
	
	
	glGenBuffers(1, &tri->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, tri->vbo);
	glBufferData(GL_ARRAY_BUFFER, len * 2 * 3 * sizeof(TextVertex), tri->vertices, GL_STATIC_DRAW);
	
	
	// init shaders elsewhere
	
	
}



void FreeFont(TextRes* res) {
	free(res->texture);
	free(res->codeIndex);
	free(res->offsets);
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


