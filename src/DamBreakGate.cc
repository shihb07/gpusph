/*  Copyright 2011-2013 Alexis Herault, Giuseppe Bilotta, Robert A. Dalrymple, Eugenio Rustico, Ciro Del Negro

    Istituto Nazionale di Geofisica e Vulcanologia
        Sezione di Catania, Catania, Italy

    Università di Catania, Catania, Italy

    Johns Hopkins University, Baltimore, MD

    This file is part of GPUSPH.

    GPUSPH is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    GPUSPH is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GPUSPH.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cmath>
#include <iostream>

#include "DamBreakGate.h"
#include "Cube.h"
#include "Point.h"
#include "Vector.h"
#include "GlobalData.h"

#define SIZE_X		(1.60)
#define SIZE_Y		(0.67)
#define SIZE_Z		(0.40)

// default: origin in 0,0,0
#define ORIGIN_X	(0)
#define ORIGIN_Y	(0)
#define ORIGIN_Z	(0)

// centered domain: use to improve accuracy
// #define ORIGIN_X	(- SIZE_X / 2)
// #define ORIGIN_Y	(- SIZE_Y / 2)
// #define ORIGIN_Z	(- SIZE_Z / 2)

DamBreakGate::DamBreakGate(const GlobalData *_gdata) : Problem(_gdata)
{
	// Size and origin of the simulation domain
	m_size = make_double3(SIZE_X, SIZE_Y, SIZE_Z + 0.7);
	m_origin = make_double3(ORIGIN_X, ORIGIN_Y, ORIGIN_Z);

	// SPH parameters
	set_deltap(0.015f);
	m_simparams.dt = 0.0001f;
	m_simparams.xsph = false;
	m_simparams.dtadapt = true;
	m_simparams.dtadaptfactor = 0.3;
	m_simparams.buildneibsfreq = 10;
	m_simparams.shepardfreq = 0;
	m_simparams.mlsfreq = 10;
	m_simparams.visctype = ARTVISC;//DYNAMICVISC//SPSVISC;
	m_simparams.mbcallback = true;
	m_simparams.boundarytype= LJ_BOUNDARY;
	m_simparams.usedem= false;
	m_simparams.tend = 10.f;

	// Free surface detection
	m_simparams.surfaceparticle = false;
	m_simparams.savenormals = false;

	// Physical parameters
	H = 0.4f;
	m_physparams.gravity = make_float3(0.0, 0.0, -9.81f);
	float g = length(m_physparams.gravity);
	m_physparams.set_density(0,1000.0, 7.0f, 20.f);
	m_physparams.numFluids = 1;

    //set p1coeff,p2coeff, epsxsph here if different from 12.,6., 0.5
	m_physparams.dcoeff = 5.0f*g*H;
	m_physparams.r0 = m_deltap;

	// BC when using MK boundary condition: Coupled with m_simsparams.boundarytype=MK_BOUNDARY
	#define MK_par 2
	m_physparams.MK_K = g*H;
	m_physparams.MK_d = 1.1*m_deltap/MK_par;
	m_physparams.MK_beta = MK_par;
	#undef MK_par

	m_physparams.kinematicvisc = 1.0e-6f;
	m_physparams.artvisccoeff = 0.3f;
	m_physparams.epsartvisc = 0.01*m_simparams.slength*m_simparams.slength;

	// Drawing and saving times
	set_timer_tick(0.002f);
	add_writer(VTKWRITER, 100);

	// Set up callback function
	m_simparams.mbcallback = true;
	MbCallBack& mbgatedata = m_mbcallbackdata[0];
	m_mbnumber = 1;
	mbgatedata.origin = make_float3(0.4 + 2*m_physparams.r0, 0, 0);
	mbgatedata.type = GATEPART;
	mbgatedata.tstart = 0.2f;
	mbgatedata.tend = 0.6f;
	mbgatedata.vel = make_float3(0.0, 0.0, 0.0);
	// Call mb_callback a first time to initialize values set by the call back function
	mb_callback(0.0, 0.0, 0);

	// Name of problem used for directory creation
	m_name = "DamBreakGate";
}


DamBreakGate::~DamBreakGate(void)
{
	release_memory();
}


void DamBreakGate::release_memory(void)
{
	parts.clear();
	gate_parts.clear();
	obstacle_parts.clear();
	boundary_parts.clear();
}


MbCallBack& DamBreakGate::mb_callback(const float t, const float dt, const int i)
{
	MbCallBack& mbgatedata = m_mbcallbackdata[0];
	if (t >= mbgatedata.tstart && t < mbgatedata.tend) {
		mbgatedata.vel = make_float3(0.0, 0.0, 4.*(t - mbgatedata.tstart));
		mbgatedata.disp += mbgatedata.vel*dt;
		}
	else
		mbgatedata.vel = make_float3(0.0f);

	return m_mbcallbackdata[0];
}

int DamBreakGate::fill_parts()
{
	float r0 = m_physparams.r0;

	Cube fluid, fluid1, fluid2, fluid3, fluid4;

	experiment_box = Cube(Point(ORIGIN_X, ORIGIN_Y, ORIGIN_Z), Vector(1.6, 0, 0),
						Vector(0, 0.67, 0), Vector(0, 0, 0.4));

	MbCallBack& mbgatedata = m_mbcallbackdata[0];
	Rect gate = Rect (Point(mbgatedata.origin) + Point(ORIGIN_X, ORIGIN_Y, ORIGIN_Z), Vector(0, 0.67, 0),
				Vector(0,0,0.4));

	obstacle = Cube(Point(0.9 + ORIGIN_X, 0.24 + ORIGIN_Y, r0 + ORIGIN_Z), Vector(0.12, 0, 0),
					Vector(0, 0.12, 0), Vector(0, 0, 0.4 - r0));

	fluid = Cube(Point(r0 + ORIGIN_X, r0 + ORIGIN_Y, r0 + ORIGIN_Z), Vector(0.4, 0, 0),
				Vector(0, 0.67 - 2*r0, 0), Vector(0, 0, 0.4 - r0));

	bool wet = false;	// set wet to true have a wet bed experiment
	if (wet) {
		fluid1 = Cube(Point(0.4 + m_deltap + r0 + ORIGIN_X, r0 + ORIGIN_Y, r0 + ORIGIN_Z), Vector(0.5 - m_deltap - 2*r0, 0, 0),
					Vector(0, 0.67 - 2*r0, 0), Vector(0, 0, 0.03));

		fluid2 = Cube(Point(1.02 + r0  + ORIGIN_X, r0 + ORIGIN_Y, r0 + ORIGIN_Z), Vector(0.58 - 2*r0, 0, 0),
					Vector(0, 0.67 - 2*r0, 0), Vector(0, 0, 0.03));

		fluid3 = Cube(Point(0.9 + ORIGIN_X , m_deltap  + ORIGIN_Y, r0 + ORIGIN_Z), Vector(0.12, 0, 0),
					Vector(0, 0.24 - 2*r0, 0), Vector(0, 0, 0.03));

		fluid4 = Cube(Point(0.9 + ORIGIN_X , 0.36 + m_deltap  + ORIGIN_Y, r0 + ORIGIN_Z), Vector(0.12, 0, 0),
					Vector(0, 0.31 - 2*r0, 0), Vector(0, 0, 0.03));
	}

	boundary_parts.reserve(2000);
	parts.reserve(14000);
	gate_parts.reserve(2000);

	experiment_box.SetPartMass(r0, m_physparams.rho0[0]);
	experiment_box.FillBorder(boundary_parts, r0, false);

	gate.SetPartMass(GATEPART);
	gate.Fill(gate_parts, r0, true);

	obstacle.SetPartMass(r0, m_physparams.rho0[0]);
	obstacle.FillBorder(obstacle_parts, r0, true);

	fluid.SetPartMass(m_deltap, m_physparams.rho0[0]);
	fluid.Fill(parts, m_deltap, true);

	if (wet) {
		fluid1.SetPartMass(m_deltap, m_physparams.rho0[0]);
		fluid1.Fill(parts, m_deltap, true);
		fluid2.SetPartMass(m_deltap, m_physparams.rho0[0]);
		fluid2.Fill(parts, m_deltap, true);
		fluid3.SetPartMass(m_deltap, m_physparams.rho0[0]);
		fluid3.Fill(parts, m_deltap, true);
		fluid4.SetPartMass(m_deltap, m_physparams.rho0[0]);
		fluid4.Fill(parts, m_deltap, true);
	}

	return parts.size() + boundary_parts.size() + obstacle_parts.size() + gate_parts.size();
}

void DamBreakGate::copy_to_array(BufferList &buffers)
{
	float4 *pos = buffers.getData<BUFFER_POS>();
	hashKey *hash = buffers.getData<BUFFER_HASH>();
	float4 *vel = buffers.getData<BUFFER_VEL>();
	particleinfo *info = buffers.getData<BUFFER_INFO>();

	std::cout << "Boundary parts: " << boundary_parts.size() << "\n";
	for (uint i = 0; i < boundary_parts.size(); i++) {
		vel[i] = make_float4(0, 0, 0, m_physparams.rho0[0]);
		info[i] = make_particleinfo(BOUNDPART,0,i);
		calc_localpos_and_hash(boundary_parts[i], info[i], pos[i], hash[i]);
	}
	int j = boundary_parts.size();
	std::cout << "Boundary part mass:" << pos[j-1].w << "\n";

	std::cout << "Gate parts: " << gate_parts.size() << "\n";
	for (uint i = j; i < j + gate_parts.size(); i++) {
		vel[i] = make_float4(0, 0, 0, m_physparams.rho0[0]);
		info[i] = make_particleinfo(GATEPART,0,i);
		calc_localpos_and_hash(gate_parts[i-j], info[i], pos[i], hash[i]);
	}
	j += gate_parts.size();
	std::cout << "Gate part mass:" << pos[j-1].w << "\n";

	std::cout << "Obstacle parts: " << obstacle_parts.size() << "\n";
	for (uint i = j; i < j + obstacle_parts.size(); i++) {
		vel[i] = make_float4(0, 0, 0, m_physparams.rho0[0]);
		info[i] = make_particleinfo(BOUNDPART,1,i);
		calc_localpos_and_hash(obstacle_parts[i-j], info[i], pos[i], hash[i]);
	}
	j += obstacle_parts.size();
	std::cout << "Obstacle part mass:" << pos[j-1].w << "\n";

	std::cout << "Fluid parts: " << parts.size() << "\n";
	for (uint i = j; i < j + parts.size(); i++) {
		vel[i] = make_float4(0, 0, 0, m_physparams.rho0[0]);
		info[i] = make_particleinfo(FLUIDPART,0,i);
		calc_localpos_and_hash(parts[i-j], info[i], pos[i], hash[i]);
	}
	j += parts.size();
	std::cout << "Fluid part mass:" << pos[j-1].w << "\n";
}

