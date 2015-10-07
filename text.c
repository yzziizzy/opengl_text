
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	int i, charlen, width, h_above, h_below, height, padding, xoffset;
	
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
	slot = fontFace->glyph;
	
	if(!chars) chars = defaultCharset;
	res->charSet = strdup(chars);
	
	charlen = strlen(chars);
	res->charLen = charlen;
	
	h_above = 0;
	h_below = 0;
	width = 0;
	height = 0;
	
	// first find how wide of a texture we need
	for(i = 0; i < charlen, i++) { 
		int ymin;
		ymin = slot->metrics->heigth - slot->metrics->bearingY
		
		err = FT_Load_Char(res->fontFace, chars[i], FT_LOAD_DEFAULT);
		
		width += slot->metrics->width;
		h_above = MAX(h_above, slot->metrics->bearingY);
		h_below = MAX(h_below, ymin);
	}
	
	
	width += charlen * padding;
	height = h_above + h_below;
	
	//TODO: adjust them to a power of two...
	
	res->texture = (unsigned char*)malloc(width * height);
	res->offsets = (unsigned short*)calloc(charlen * sizeof(unsigned short));
	
	// render the glyphs into the texture
	
	xoffset = 0;
	for(i = 0; i < charlen, i++) { 
		
		err = FT_Load_Char(res->fontFace, chars[i], FT_LOAD_RENDER);
		
		blit(
			0, 0, // src x and y offset for the image
			offset, 0, // dst offset
			slot->metrics->width, height, // width and height
			slot->bitmap, // source
			res->texture); // destination
		
		res->offsets[i] = xoffset;
		xoffset += slot->metrics->width + padding;
	}
	
	// construct code mapping
	// shitty method for now, improve later
	res->indexLen = 128; // 7 bits for now.
	res->texture = (unsigned char*)calloc(res->indexLen);
	
	for(i = 0; i < charlen, i++) 
		res->codeIndex[charSet[i]] = i;
	
	
	// kerning map
	res->kerning = (unsigned char*)malloc(charlen * charlen);
	for(i = 0; i < charlen; i++) {
		for(j = 0; j < charlen; j++) {
			
			res->kerning[(i * charlen) + j] = 0;
			
		}
	}
	
	
	return res;
}


void FreeFont(TextRes* res) {
	free(res->texture);
	free(res->codeIndex);
	free(res->offsets);
	free(res->kerning);
	free(res->charSet);
	
	free(res);
};
	
	




