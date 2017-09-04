////////////////////////////////////////////////////////////////////////////////
//
// Filename:	topolar_tb.cpp
//
// Project:	A series of CORDIC related projects
//
// Purpose:	A quick test bench to determine if the rectangular to polar
//		cordic module works.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2017, Gisselquist Technology, LLC
//
// This program is free software (firmware): you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or (at
// your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program.  (It's in the $(ROOT)/doc directory.  Run make with no
// target there if the PDF file isn't present.)  If not, see
// <http://www.gnu.org/licenses/> for a copy.
//
// License:	GPL, v3, as defined and found on www.gnu.org,
//		http://www.gnu.org/licenses/gpl.html
//
//
////////////////////////////////////////////////////////////////////////////////
//
//
#include <stdio.h>

#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vtopolar.h"
#include "topolar.h"
#include "testb.h"

class	TOPOLAR_TB : public TESTB<Vtopolar> {
	bool		m_debug;
public:

	TOPOLAR_TB(void) {
		m_debug = true;
		m_core->i_reset = 1;
		m_core->i_ce    = 1;
		m_core->i_xval  = 0;
		m_core->i_yval  = 0;
		m_core->i_aux   = 0;
		tick();
	}
};

const int	LGNSAMPLES=15;
const int	NSAMPLES=(1<<LGNSAMPLES);

int main(int  argc, char **argv) {
	Verilated::commandArgs(argc, argv);
	TOPOLAR_TB	*tb = new TOPOLAR_TB;
	int	ipdata[NSAMPLES], ixval[NSAMPLES], iyval[NSAMPLES],
		imag[NSAMPLES], ophase[NSAMPLES], omag[NSAMPLES],
		idx, shift, pshift;
	double	dpdata[NSAMPLES];

	tb->opentrace("topolar_tb.vcd");
	tb->reset();

	shift  = (8*sizeof(long)-OW);
	pshift = (8*sizeof(long)-PW);
	idx = 0;
	for(int i=0; i<NSAMPLES; i++) {
		double	ph, cs, sn, mg;
		long	lv;

		lv = (((long)i) << (PW-(LGNSAMPLES-1)));
		ipdata[i] = (int)lv;
		ph = ipdata[i] * M_PI / (1ul << (PW-1));
		mg = ((1l<<(IW-1))-1);
		cs = mg * cos(ph);
		sn = mg * sin(ph);

		ixval[i] = (int)cs;
		iyval[i] = (int)sn;
		imag[i]  = (int)mg;
		// dpdata[i] = atan2(iyval[i], ixval[i]);
		dpdata[i] = ph;
		tb->m_core->i_xval  = ixval[i];
		tb->m_core->i_yval  = iyval[i];
		tb->m_core->i_aux   = 1;
		tb->tick();

		if (tb->m_core->o_aux) {
			long	lv;
			lv = (long)tb->m_core->o_mag;
			lv <<= shift;
			lv >>= shift;
			omag[idx]   = (int)lv;

			lv = tb->m_core->o_phase;
			lv <<= pshift;
			lv >>= pshift;
			ophase[idx] = (int)lv;
			//printf("%08x %08x -> %08x %08x\n",
			//	ixval[idx], iyval[idx],
			//	omag[idx], ophase[idx]);
			idx++;
		}
	}

	tb->m_core->i_aux = 0;
	while(tb->m_core->o_aux) {
		tb->m_core->i_aux   = 0;
		tb->tick();

		if (tb->m_core->o_aux) {
			long	lv;
			lv = (long)tb->m_core->o_mag;
			lv <<= shift;
			lv >>= shift;
			omag[idx]   = (int)lv;

			lv = tb->m_core->o_phase;
			lv <<= pshift;
			lv >>= pshift;
			ophase[idx] = (int)lv;
			//printf("%08x %08x -> %08x %08x\n",
			//	ixval[idx], iyval[idx],
			//	omag[idx], ophase[idx]);
			idx++;
		}
	}

	double	mxperr = 0.0, mxverr = 0.0,
		maxphase = pow(2.0,PW);
	for(int i=0; i<NSAMPLES; i++) {
		double	mgerr, epdata, dperr, emag;

		epdata = dpdata[i] / 2.0 / M_PI * maxphase;
		if (epdata < 0.0)
			epdata += maxphase;
		dperr = ophase[i] - epdata;
		while (dperr > maxphase/2.)
			dperr -= maxphase;
		while (dperr < -maxphase/2.)
			dperr += maxphase;
		if (dperr > mxperr)
			mxperr = dperr;

		emag = imag[i] * GAIN;// * sqrt(2);
		if (IW+1 > OW)
			emag = emag / pow(2.,(IW-1-OW))/4/sqrt(2);
		else if (OW > IW+1)
			emag = emag * pow(2.,(IW-1-OW));

		// omag should equal imag * GAIN
		mgerr = fabs(omag[i] - emag);
		if (mgerr > mxverr)
			mxverr = mgerr;

		if (epdata > maxphase/2)
			epdata -= maxphase;
		//printf("%08x %08x -> %6d %08x/%12d [%9.6f %12.1f],[%9.6f %13.1f]\n",
		//	ixval[i], iyval[i],
		//	omag[i], ophase[i],ophase[i],
		//	emag, epdata,
		//	mgerr, dperr);
	}

	if(true) {
		FILE	*dbgfp;
		dbgfp = fopen("topolar.32t", "w");
		assert(dbgfp);

		for(int k=0; k<NSAMPLES; k++) {
			int	ovals[5];
			double	epdata, dperr;
			ovals[0] = ixval[k];
			ovals[1] = iyval[k];
			ovals[2] = omag[k];
			ovals[3] = ophase[k];

			epdata = dpdata[k] / 2.0 / M_PI * maxphase;
			dperr = ophase[k] - epdata;
			while (dperr > maxphase/2.)
				dperr -= maxphase;
			while (dperr < -maxphase/2.)
				dperr += maxphase;
			ovals[4] = (int)dperr;
			fwrite(ovals, sizeof(ovals[0]), sizeof(ovals)/sizeof(ovals[0]), dbgfp);
		}

		fclose(dbgfp);
	}

	bool	failed_test = false;
	double	expected_phase_err;

	// First phase error: based upon the smallest arctan difference
	// between samples.
	expected_phase_err = atan2(1,(1<<(IW-1)))*maxphase / M_PI / 2.;
	expected_phase_err *= expected_phase_err;
	// Plus the quantization error in the phase calculation
	expected_phase_err += QUANTIZATION_VARIANCE;
	// Plus any truncation error in the in the phase values
	expected_phase_err += PHASE_VARIANCE_RAD
				* ((1.0*(1<<PW)) / M_PI / 2.)
				* ((1.0*(1<<PW)) / M_PI / 2.);
	expected_phase_err = sqrt(expected_phase_err);
	if (expected_phase_err < 1.0)
		expected_phase_err = 1.0;
	if (mxperr > 2.0 * expected_phase_err)
		failed_test = true;

	if (mxverr > 2.0 * sqrt(QUANTIZATION_VARIANCE))
		failed_test = true;

	printf("Max phase     error: %.2f (%.6f Rel), expect %.2f\n", mxperr,
		mxperr / (2.0 * (1<<(PW-1))),
		expected_phase_err);
	printf("Max magnitude error: %9.6f, expect %.2f\n", mxverr,
		sqrt(QUANTIZATION_VARIANCE));

	if (failed_test) {
		printf("TEST FAILED!!\n");
		exit(EXIT_FAILURE);
	} else {
		printf("SUCCESS\n");
		exit(EXIT_SUCCESS);
	}
}
