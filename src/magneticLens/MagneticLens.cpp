//----------------------------------------------------------------------
// This file is part of PARSEC (http://physik.rwth-aachen.de/parsec)
// a parametrized simulation engine for cosmic rays.
//
// Copyright (C) 2011  Martin Erdmann, Peter Schiffer, Tobias Winchen
//                     RWTH Aachen University, Germany
// Contact: winchen@physik.rwth-aachen.de
//
//  This program is free software: you can redistribute it and/or
//  modify it under the terms of the GNU General Public License as
//  published by the Free Software Foundation, either version 3 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------

#include "crpropa/magneticLens/MagneticLens.h"

#include "crpropa/Random.h"
#include "crpropa/Units.h"

// needed for memcpy in gcc 4.3.2
#include <cstring>

namespace crpropa 
{

void MagneticLens::loadLens(const string &filename)
{
	ifstream infile(filename.c_str());
	if (!infile)
	{
		throw std::runtime_error("Can't read file: " + filename);
	}
	string line;

	string prefix;
	int sp = filename.find_last_of("/");
	if (sp >= 0)
	{
		prefix = filename.substr(0, sp);
		prefix.append("/");
	}
	string mfdatfile;
	double emin, emax;

	int lineCounter = 0;
	while (!infile.eof())
	{
		getline(infile, line);
		lineCounter++;
		if (line.find('#') == string::npos)
		{
			stringstream ss;
			ss << line;
			ss >> mfdatfile >> emin >> emax;
			if (ss.fail())
			{
				cerr << "WARNING! Cannot read line " << lineCounter << " of file " << filename << " :\n [" << lineCounter << " ]: " << line << " \n";
				cerr << " ... Skipping line and continue\n";
			}
			else
			{
				this->loadLensPart(prefix + mfdatfile, pow(10, emin) * eV, pow(10, emax) * eV);
			}
		}
	}
}

bool MagneticLens::transformCosmicRay(double rigidity, double& phi,
		double& theta) 
{
	uint32_t c = _pixelization->direction2Pix(phi, theta);
	LensPart *lenspart = getLensPart(rigidity);
	if (!lenspart)
	{
		std::cerr << "Warning. Trying to transform cosmic ray with rigidity " << rigidity / eV << " eV which is not covered by this lens!.\n";
		std::cerr << " This lens covers the range " << _minimumRigidity /eV << " eV - " << _maximumRigidity << " eV.\n";
		return false;
	}

	ModelVectorType v = lenspart->getMatrix().col(c);

	uint32_t r;

	// the random number to compare with
	double rn = Random::instance().rand();

	ModelVectorType::InnerIterator i(v);
  double cpv = 0;
  while (i)
  {
		cpv += i.value();
		if (rn < cpv)
		{
			_pixelization->pix2Direction(i.index(), phi, theta);
			return true;
    }
    else
    {
			++i;
    }
   }
  return false;
}

bool MagneticLens::transformCosmicRay(double rigidity, Vector3d &p){

			double galacticLongitude = atan2(-p.y, -p.x);
			double galacticLatitude =	M_PI / 2 - acos(-p.z/ sqrt(p.x*p.x + p.y*p.y + p.z*p.z));
			bool result = transformCosmicRay(rigidity, galacticLongitude, galacticLatitude);
			
			p.x = -1 * cos(galacticLongitude) * sin(M_PI / 2 - galacticLatitude);
			p.y = -1 * sin(galacticLongitude) * sin(M_PI / 2 - galacticLatitude);
			p.z = -1. * cos(M_PI / 2 - galacticLatitude);

			return result;
}

void MagneticLens::loadLensPart(const string &filename, double rigidityMin,
		double rigidityMax)
{
	updateRigidityBounds(rigidityMin, rigidityMax);

	LensPart *p = new LensPart(filename, rigidityMin, rigidityMax);
	p->loadMatrixFromFile();
	_checkMatrix(p->getMatrix());

	_lensParts.push_back(p);
}

void MagneticLens::_checkMatrix(const ModelMatrixType &M)
{
	if (M.rows() != M.cols())
	{
		throw std::runtime_error("Not a square Matrix!");
	}

	if (_pixelization)
	{
		if (_pixelization->nPix() != M.cols())
		{
			std::cerr << "*** ERROR ***" << endl;
			std::cerr << "  Pixelization: " << _pixelization->nPix() << endl;
			std::cerr << "  Matrix Size : " << M.cols() << endl;
			throw std::runtime_error("Matrix doesn't fit into Lense");
		}
	}
	else
	{
		uint32_t morder = Pixelization::pix2Order(M.cols());
		if (morder == 0)
		{
			throw std::runtime_error(
					"Matrix size doesn't match healpix scheme!");
		}
		_pixelization = new Pixelization(morder);
	}
}

void MagneticLens::updateRigidityBounds(double rigidityMin, double rigidityMax)
{
	if (rigidityMin >= rigidityMax)
	{
		throw std::runtime_error("rigidityMin >= rigidityMax");
	}
	if (rigidityMin < _minimumRigidity)
	{
		_minimumRigidity = rigidityMin;
	}

	if (_maximumRigidity < rigidityMin)
	{
		_maximumRigidity = rigidityMax;
	}
}

void MagneticLens::setLensPart(const ModelMatrixType &M, double rigidityMin,
		double rigidityMax)
{
	updateRigidityBounds(rigidityMin, rigidityMax);
	LensPart *p = new LensPart("Direct Input", rigidityMin, rigidityMax);

	p->setMatrix(M);

	_checkMatrix(p->getMatrix());
	_lensParts.push_back(p);
}

LensPart* MagneticLens::getLensPart(double rigidity) const
{
	const_LensPartIter i = _lensParts.begin();
	while (i != _lensParts.end())
	{
		if (((*i)->getMinimumRigidity() < rigidity / eV)
				&& ((*i)->getMaximumRigidity() >= rigidity / eV))
		{
			return (*i);
		}
		++i;
	}
	return NULL;
}

bool MagneticLens::rigidityCovered(double rigidity) const
{
	if (getLensPart(rigidity))
		return true;
	else
		return false;
}


void MagneticLens::normalizeMatrixColumns()
{
	for (LensPartIter iter = _lensParts.begin(); iter != _lensParts.end();
			++iter)
	{
		normalizeColumns((*iter)->getMatrix());
	}
}


void MagneticLens::normalizeLens()
{
	// get maximum of sums of columns, and normalize each matrix to that
	double norm = 0;
	for (LensPartIter iter = _lensParts.begin(); iter != _lensParts.end();
			++iter)
	{
		if ((*iter)->getMaximumOfSumsOfColumns() > norm)
		{
			norm = (*iter)->getMaximumOfSumsOfColumns();
		}
	}
	for (LensPartIter iter = _lensParts.begin(); iter != _lensParts.end();
			++iter)
	{
		normalizeMatrix((*iter)->getMatrix(), norm);
	}
  _norm = norm;
}

void MagneticLens::normalizeLensparts()
{
	double norm;
	for (LensPartIter iter = _lensParts.begin(); iter != _lensParts.end();
			++iter)
	{
		norm = (*iter)->getMaximumOfSumsOfColumns();
		normalizeMatrix((*iter)->getMatrix(), norm);
	}
}

void MagneticLens::transformModelVector(double* model, double rigidity) const
{
	LensPart* lenspart = getLensPart(rigidity);
	
	if (!lenspart)
	{
		std::cerr << "Warning. Trying to transform vector with rigidity " << rigidity / eV << "eV which is not covered by this lens!.\n" << std::endl;
		return;
	}

	prod_up(lenspart->getMatrix(), model);

}



} // namespace parsec

