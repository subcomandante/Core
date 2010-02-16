/****************************************************************************
 * 			textureback.cc: a background using the texture class
 *      This is part of the yafray package
 *      Copyright (C) 2006  Mathias Wein
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2.1 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library; if not, write to the Free Software
 *      Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
 
#include <yafray_config.h>

#include <core_api/environment.h>
#include <core_api/background.h>
#include <core_api/texture.h>
#include <core_api/light.h>
#include <utilities/sample_utils.h>
#include <lights/bglight.h>

__BEGIN_YAFRAY

class textureBackground_t: public background_t
{
	public:
		enum PROJECTION { spherical=0, angular };
		textureBackground_t(const texture_t *texture, PROJECTION proj, bool doIBL, int nsam, CFLOAT bpower, float rot, bool shootC, bool shootD);
		virtual color_t operator() (const ray_t &ray, renderState_t &state, bool filtered=false) const;
		virtual color_t eval(const ray_t &ray, bool filtered=false) const;
		virtual light_t* getLight() const { return envLight; }
		virtual ~textureBackground_t();
		static background_t *factory(paraMap_t &,renderEnvironment_t &);

	protected:
		const texture_t *tex;
		bool ibl; //!< indicate wether to do image based lighting
		PROJECTION project;
		light_t *envLight;
		CFLOAT power;
		float rotation;
		PFLOAT sin_r, cos_r;
		bool shootCaustic;
		bool shootDiffuse;
};

class constBackground_t: public background_t
{
	public:
		constBackground_t(color_t col, bool ibl, int iblsamples);
		virtual color_t operator() (const ray_t &ray, renderState_t &state, bool filtered=false) const;
		virtual color_t eval(const ray_t &ray, bool filtered=false) const;
		virtual light_t* getLight() const { return envLight; }
		virtual ~constBackground_t();
		static background_t *factory(paraMap_t &params,renderEnvironment_t &render);
	protected:
		color_t color;
		light_t *envLight;
};


textureBackground_t::textureBackground_t(const texture_t *texture, PROJECTION proj, bool IBL, int nsam, CFLOAT bpower, float rot, bool shootC, bool shootD):
	tex(texture), ibl(IBL), project(proj), envLight(0), power(bpower)
{
	rotation = 2.0f * rot / 360.f;
	sin_r = fSin(M_PI*rotation);
	cos_r = fCos(M_PI*rotation);
	
	if(ibl) envLight = new bgLight_t(this, nsam, shootC, shootD);
}

textureBackground_t::~textureBackground_t()
{
	if(envLight) delete envLight;
}

color_t textureBackground_t::operator() (const ray_t &ray, renderState_t &state, bool filtered) const
{
	return eval(ray);
}

color_t textureBackground_t::eval(const ray_t &ray, bool filtered) const
{
	PFLOAT u = 0.f, v = 0.f;
	
	if (project == angular)
	{
		point3d_t dir(ray.dir);
		dir.x = ray.dir.x * cos_r + ray.dir.y * sin_r;
		dir.y = ray.dir.x * -sin_r + ray.dir.y * cos_r;
		angmap(dir, u, v);
	}
	else
	{
		spheremap(ray.dir, u, v);//this returns u,v in 0,1 range (useful for bgLight_t)
		//put u,v in -1,1 range
		u = 2.f * u - 1.f;
		v = 2.f * v - 1.f;
		u += rotation;
		if (u > 1.f) u -= 2.f;
	}
	
	color_t ret = tex->getColor(point3d_t(u, v, 0.f));
	
	if(ret.minimum() < 1e-6f) ret = color_t(1e-5f);
	
	return power * ret;
}

background_t* textureBackground_t::factory(paraMap_t &params,renderEnvironment_t &render)
{
	const texture_t *tex=0;
	const std::string *texname=0;
	const std::string *mapping=0;
	PROJECTION pr = spherical;
	double power = 1.0, rot=0.0;
	bool IBL = false;
	int IBL_sam = 8; //quite arbitrary really...
	bool caust = true;
	bool diffuse = true;
	
	if( !params.getParam("texture", texname) )
	{
		std::cerr << "error: no texture given for texture background!";
		return 0;
	}
	tex = render.getTexture(*texname);
	if( !tex )
	{
		std::cerr << "error: texture '"<<*texname<<"' for textureback not existant!\n";
		return 0;
	}
	if( params.getParam("mapping", mapping) )
	{
		if(*mapping == "probe" || *mapping == "angular") pr = angular;
	}
	params.getParam("ibl", IBL);
	params.getParam("ibl_samples", IBL_sam);
	params.getParam("power", power);
	params.getParam("rotation", rot);
	params.getParam("with_caustic", caust);
	params.getParam("with_diffuse", diffuse);
	return new textureBackground_t(tex, pr, IBL, IBL_sam, (CFLOAT)power, float(rot), caust, diffuse);
}

/* ========================================
/ minimalistic background...
/ ========================================= */

constBackground_t::constBackground_t(color_t col, bool ibl, int iblsamples):color(col), envLight(0)
{
	if(ibl) envLight = new bgLight_t(this, iblsamples, false, true);
}
constBackground_t::~constBackground_t()
{
	if(envLight) delete envLight;
}

color_t constBackground_t::operator() (const ray_t &ray, renderState_t &state, bool filtered) const
{
	return color;
}

color_t constBackground_t::eval(const ray_t &ray, bool filtered) const
{
	return color;
}

background_t* constBackground_t::factory(paraMap_t &params,renderEnvironment_t &render)
{
	color_t col(0.f);
	float power = 1.0;
	int IBL_sam = 8; //strandarized wild guess
	bool IBL = false;
	
	params.getParam("color", col);
	params.getParam("power", power);
	params.getParam("ibl", IBL);
	params.getParam("ibl_samples", IBL_sam);
	
	return new constBackground_t(col*power, IBL, IBL_sam);
}

extern "C"
{
	
	YAFRAYPLUGIN_EXPORT void registerPlugin(renderEnvironment_t &render)
	{
		render.registerFactory("textureback",textureBackground_t::factory);
		render.registerFactory("constant", constBackground_t::factory);
	}

}
__END_YAFRAY
