/*
 * Copyright (c) 2009-2015, Albertas Vyšniauskas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of the software author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ColorNames.h"
#include "../MathUtil.h"
#include <math.h>
#include <string.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace std;

//CIE94 color difference calculation
float color_names_distance_lch(const Color* a, const Color* b)
{
	Color al, bl;
	color_lab_to_lch(a, &al);
	color_lab_to_lch(b, &bl);
	return sqrt(
		pow((bl.lch.L - al.lch.L) / 1, 2) +
		pow((bl.lch.C - al.lch.C) / (1 + 0.045 * al.lch.C), 2) +
		pow((pow(a->lab.a - b->lab.a, 2) + pow(a->lab.b - b->lab.b, 2) - (bl.lch.C - al.lch.C)) / (1 + 0.015 * al.lch.C), 2));
}
ColorNames* color_names_new()
{
	ColorNames* cnames = new ColorNames;
	cnames->color_space_convert = color_rgb_to_lab_d50;
	cnames->color_space_distance = color_names_distance_lch;
	return cnames;
}
static void color_names_strip_spaces(string& string_x, string& stripchars)
{
	if(string_x.empty()) return;
	if (stripchars.empty()) return;
	size_t startIndex = string_x.find_first_not_of(stripchars);
	size_t endIndex = string_x.find_last_not_of(stripchars);
	if ((startIndex==string::npos)||(endIndex==string::npos)){
		string_x.erase();
		return;
	}
	string_x = string_x.substr(startIndex, (endIndex-startIndex)+1 );
}
void color_names_get_color_xyz(ColorNames* cnames, Color* c, int* x1, int* y1, int* z1, int* x2, int* y2, int* z2)
{
	*x1 = clamp_int(int(c->xyz.x * 8 - 0.5), 0, 7);
	*y1 = clamp_int(int(c->xyz.y * 8 - 0.5), 0, 7);
	*z1 = clamp_int(int(c->xyz.z * 8 - 0.5), 0, 7);
	*x2 = clamp_int(int(c->xyz.x * 8 + 0.5), 0, 7);
	*y2 = clamp_int(int(c->xyz.y * 8 + 0.5), 0, 7);
	*z2 = clamp_int(int(c->xyz.z * 8 + 0.5), 0, 7);
}
list<ColorEntry*>* color_names_get_color_list(ColorNames* cnames, Color* c)
{
	int x,y,z;
	x = clamp_int(int(c->xyz.x * 8), 0, 7);
	y = clamp_int(int(c->xyz.y * 8), 0, 7);
	z = clamp_int(int(c->xyz.z * 8), 0, 7);
	return &cnames->colors[x][y][z];
}
int color_names_load_from_file(ColorNames* cnames, const char* filename)
{
	ifstream file(filename, ifstream::in);
	if (file.is_open()) {
		string line;
		stringstream rline (ios::in | ios::out);
		Color color;
		string name;
		string strip_chars = " \t,.\n\r";
		while (!(file.eof())) {
			getline(file, line);
			if (line.empty()) continue;
			if (line.at(0) == '!') continue;
			rline.clear();
			rline.str(line);
			rline >> color.rgb.red >> color.rgb.green >> color.rgb.blue;
			getline(rline, name);
			color_names_strip_spaces(name, strip_chars);
			string::iterator i(name.begin());
			if (i != name.end())
				name[0] = toupper((unsigned char)name[0]);
			while(++i != name.end()){
				*i = tolower((unsigned char)*i);
			}
			color_multiply(&color, 1/255.0);
			ColorNameEntry* name_entry = new ColorNameEntry;
			name_entry->name=name;
			cnames->names.push_back(name_entry);
			ColorEntry* color_entry = new ColorEntry;
			color_entry->name=name_entry;
			cnames->color_space_convert(&color, &color_entry->color);
			color_names_get_color_list(cnames, &color_entry->color)->push_back(color_entry);
		}
		file.close();
		return 0;
	}
	return -1;
}
void color_names_destroy(ColorNames* cnames)
{
	for (list<ColorNameEntry*>::iterator i=cnames->names.begin();i!=cnames->names.end();++i){
		delete (*i);
	}
	for (int x=0;x<8;x++){
		for (int y=0;y<8;y++){
			for (int z=0;z<8;z++){
				for (list<ColorEntry*>::iterator i=cnames->colors[x][y][z].begin();i!=cnames->colors[x][y][z].end();++i){
					delete (*i);
				}
			}
		}
	}
	delete cnames;
}
string color_names_get(ColorNames* cnames, Color* color, bool imprecision_postfix)
{
	Color c1;
	cnames->color_space_convert(color, &c1);
	int x1, y1, z1, x2, y2, z2;
	color_names_get_color_xyz(cnames, &c1, &x1, &y1, &z1, &x2, &y2, &z2);
	float result_delta=1e5;
	ColorEntry* color_entry = NULL;
	char skip_mask[8][8][8];
	memset(&skip_mask, 0, sizeof(skip_mask));
	/* Search expansion should be from 0 to 7, but this would only increase search time and return
	 * wrong color names when no closely matching color is found. Search expansion is only useful
	 * when color name database is very small (16 colors)
	 */
	for (int expansion = 0; expansion < 7; ++expansion){
		int x_start = std::max(x1 - expansion, 0), x_end = std::min(x2 + expansion, 7);
		int y_start = std::max(y1 - expansion, 0), y_end = std::min(y2 + expansion, 7);
		int z_start = std::max(z1 - expansion, 0), z_end = std::min(z2 + expansion, 7);
		for (int x_i = x_start; x_i <= x_end; ++x_i) {
			for (int y_i = y_start; y_i <= y_end; ++y_i) {
				for (int z_i = z_start; z_i <= z_end; ++z_i) {
					if (skip_mask[x_i][y_i][z_i]) continue; // skip checked items
					skip_mask[x_i][y_i][z_i] = 1;
					for (list<ColorEntry*>::iterator i=cnames->colors[x_i][y_i][z_i].begin(); i!=cnames->colors[x_i][y_i][z_i].end();++i){
						float delta = cnames->color_space_distance(&(*i)->color, &c1);
						if (delta < result_delta) {
							result_delta=delta;
							color_entry=*i;
						}
					}
				}
			}
		}
		//no need for further expansion if we have found a match
		if (color_entry) break;
	}
	if (color_entry){
		stringstream s;
		s << color_entry->name->name;
		if (imprecision_postfix) if (result_delta>0.1) s<<" ~";
		return s.str();
	}
	return string("");
}

