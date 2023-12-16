﻿#include "nrtdsm_shared.h"
#include "../common/common_host.h"

#if ENABLE_VDB

static void solveCubicEquation(
	const float a, const float b, const float c, const float d,
	float* const x0, float* const x1, float* const x2) {
	if (a == 0.0f) {
		*x2 = NAN;
		if (b == 0) {
			*x1 = NAN;
			if (c == 0.0f) {
				// Constant
				*x0 = NAN;
				return;
			}
			// Linear
			*x0 = -d / c;
			return;
		}
		// Quadratic
		const float D = pow2(c) - 4 * c * d;
		if (D < 0.0f) {
			*x0 = *x1 = NAN;
			return;
		}
		const float sqrtD = std::sqrt(D);
		const float rec_2b = 1.0f / (2 * b);
		*x0 = (-b - sqrtD) * rec_2b;
		*x1 = (-b + sqrtD) * rec_2b;
		return;
	}
	// Cubic
	const float a3 = pow3(a);
	const float a2 = pow2(a);
	const float b3 = pow3(b);
	const float b2 = pow2(b);
	const float c3 = pow3(c);
	const float c2 = pow2(c);
	const float d2 = pow2(d);

	const float p = (-b2 + 3 * a * c) / (9 * a2);
	const float q = (2 * b3 - 9 * a * b * c + 27 * a2 * d) / (54 * a3);
	const float r2 = pow2(q) + pow3(p);
	const float b3a = -b / (3 * a);
	if (r2 > 0) {
		const float r = std::sqrt(r2);
		const float qrA = -q + r;
		const float qrB = -q - r;
		*x0 = b3a
			+ std::copysign(std::pow(std::fabs(qrA), 1.0f / 3.0f), qrA)
			+ std::copysign(std::pow(std::fabs(qrB), 1.0f / 3.0f), qrB);
		*x1 = *x2 = NAN;
	}
	else if (r2 * pow4(a) >= -1e-6) {
		*x0 = b3a + 2 * std::copysign(std::pow(std::fabs(q), 1.0f / 3.0f), -q);
		*x1 = *x2 = b3a - std::copysign(std::pow(std::fabs(q), 1.0f / 3.0f), -q);
	}
	else {
		const float r = std::sqrt(-r2);
		const float radius = std::pow(pow2(q) + pow2(r), 1.0f / 6.0f);
		const float arg = std::atan2(r, -q) / 3.0f;
		const float zr = radius * std::cos(arg);
		const float zi = radius * std::sin(arg);
		const float sqrt3 = std::sqrt(3);
		*x0 = b3a + 2 * zr;
		*x1 = b3a - zr - sqrt3 * zi;
		*x2 = b3a - zr + sqrt3 * zi;
	}

	const auto substitute = [&](const float x) {
		return a * pow3(x) + b * pow2(x) + c * x + d;
	};
	hpprintf(
		"[%g, %g, %g, %g], r2*a4: %g\n",
		a, b, c, d, r2 * pow4(a));
	hpprintf(
		"(%.6f, %.6f, %.6f) => (%.6f, %.6f, %.6f)\n",
		*x0, *x1, *x2,
		substitute(*x0), substitute(*x1), substitute(*x2));
}

// Solve the equation (4)
static void findHeight(
	const Point3D &pA, const Point3D &pB, const Point3D &pC,
	const Normal3D &nA, const Normal3D &nB, const Normal3D &nC,
	const Point3D &p,
	float* const h0, float* const h1, float* const h2) {
	const Vector3D eAB = pB - pA;
	const Vector3D eAC = pC - pA;
	const Vector3D fAB = static_cast<Vector3D>(nB - nA);
	const Vector3D fAC = static_cast<Vector3D>(nC - nA);
	const Vector3D eAp = p - pA;

	const Vector3D alpha2 = cross(fAB, fAC);
	const Vector3D alpha1 = cross(eAB, fAC) + cross(fAB, eAC);
	const Vector3D alpha0 = cross(eAB, eAC);

	const float c_a = -dot(nA, alpha2);
	const float c_b = dot(eAp, alpha2) - dot(nA, alpha1);
	const float c_c = dot(eAp, alpha1) - dot(nA, alpha0);
	const float c_d = dot(eAp, alpha0);

	/*const float c_a3 = pow3(c_a);
	const float c_a2 = pow2(c_a);
	const float c_b3 = pow3(c_b);
	const float c_b2 = pow2(c_b);
	const float c_c3 = pow3(c_c);
	const float c_c2 = pow2(c_c);
	const float c_d2 = pow2(c_d);

	const float tA = (-2 * c_b3 + 9 * c_a * c_b * c_c - 27 * c_a2 * c_d) / (54 * c_a3);
	const float _tB = (-c_b2 + 3 * c_a * c_c) / (9 * c_a2);
	const float tB = std::sqrt(pow2(tA) + pow3(_tB));

	const float h0 =
		-c_b / (3 * c_a)
		+ std::pow(tA + tB, 1.0f / 3.0f)
		+ std::pow(tA - tB, 1.0f / 3.0f);*/
	solveCubicEquation(c_a, c_b, c_c, c_d, h0, h1, h2);
}

void testFindHeight() {
	struct TestData {
		Point3D pA;
		Point3D pB;
		Point3D pC;
		Normal3D nA;
		Normal3D nB;
		Normal3D nC;
		Point2D tcA;
		Point2D tcB;
		Point2D tcC;

		Point3D SA(float h) const {
			return pA + h * nA;
		}
		Point3D SB(float h) const {
			return pB + h * nB;
		}
		Point3D SC(float h) const {
			return pC + h * nC;
		}
		Point3D p(const float alpha, const float beta) const {
			return (1 - alpha - beta) * pA + alpha * pB + beta * pC;
		}
		Normal3D n(const float alpha, const float beta) const {
			return (1 - alpha - beta) * nA + alpha * nB + beta * nC;
		}
		Point2D tc(const float alpha, const float beta) const {
			return (1 - alpha - beta) * tcA + alpha * tcB + beta * tcC;
		}
		Point3D S(const float alpha, const float beta, const float h) const {
			const Point3D ret = (1 - alpha - beta) * SA(h) + alpha * SB(h) + beta * SC(h);
			return ret;
		}
	};

	const TestData test = {
		Point3D(-0.5f, -0.4f, 0.1f),
		Point3D(0.4f, 0.1f, 0.4f),
		Point3D(-0.3f, 0.5f, 0.6f),
		normalize(Normal3D(-0.3f, -0.2f, 1.0f)),
		normalize(Normal3D(0.8f, -0.3f, 0.4f)),
		normalize(Normal3D(0.4f, 0.2f, 1.0f)),
		Point2D(0.4f, 0.7f),
		Point2D(0.2f, 0.2f),
		Point2D(0.7f, 0.4f)
	};

	vdb_frame();

	constexpr float axisScale = 1.0f;
	drawAxes(axisScale);

	constexpr bool showNegativeShell = true;

	// World-space Shell
	setColor(RGB(0.25f));
	drawWiredTriangle(test.pA, test.pB, test.pC);
	setColor(RGB(0.0f, 0.5f, 1.0f));
	drawVector(test.pA, test.nA, 1.0f);
	drawVector(test.pB, test.nB, 1.0f);
	drawVector(test.pC, test.nC, 1.0f);
	for (int i = 1; i <= 10; ++i) {
		const float p = static_cast<float>(i) / 10;
		setColor(RGB(p));
		drawWiredTriangle(test.SA(p), test.SB(p), test.SC(p));
	}
	if constexpr (showNegativeShell) {
		setColor(RGB(0.0f, 0.05f, 0.1f));
		drawVector(test.pA, test.nA, -1.0f);
		drawVector(test.pB, test.nB, -1.0f);
		drawVector(test.pC, test.nC, -1.0f);
		for (int i = 1; i <= 10; ++i) {
			const float p = -static_cast<float>(i) / 10;
			setColor(RGB(-p));
			drawWiredTriangle(test.SA(p), test.SB(p), test.SC(p));
		}
	}

	// World-space Ray
	const Point3D rayOrg(0.5f, -0.5f, 1.0f);
	const Vector3D rayDir = normalize(Vector3D(-0.7f, 1.3f, -0.5f));
	constexpr float rayLength = 2.0f;
	setColor(RGB(1.0f));
	drawCross(rayOrg, 0.05f);
	drawVector(rayOrg, rayDir, rayLength);

	// 3.3 Mapping between Two Spaces
	setColor(RGB(1.0f, 0.5f, 0));
	for (int i = 0; i <= 10; ++i) {
		const float t = static_cast<float>(i) / 10;
		const Point3D p = rayOrg + t * rayLength * rayDir;
		float hs[3];
		findHeight(
			test.pA, test.pB, test.pC,
			test.nA, test.nB, test.nC,
			p,
			&hs[0], &hs[1], &hs[2]);
		for (int j = 0; j < 3; ++j) {
			const float h = hs[j];
			if (!std::isfinite(h))
				continue;

			const Point3D SAh = test.SA(h);
			const Point3D SBh = test.SB(h);
			const Point3D SCh = test.SC(h);
			drawWiredTriangle(SAh, SBh, SCh);

			const Vector3D eAB = SBh - SAh;
			const Vector3D eAC = SCh - SAh;
			const Vector3D eAp = p - SAh;
			const float recDenom = 1.0f / (eAB.x * eAC.y - eAB.y * eAC.x);
			const float bcB = recDenom * (eAp.x * eAC.y - eAp.y * eAC.x);
			const float bcC = -recDenom * (eAp.x * eAB.y - eAp.y * eAB.x);

			drawCross((1 - bcB - bcC) * SAh + bcB * SBh + bcC * SCh, 0.025f);
		}
	}
}



// Compute coefficients of the equation (7)
static void computeCoeffs(
	const Point3D &rayOrg, const Vector3D &rayDir,
	const Point3D &pA, const Point3D &pB, const Point3D &pC,
	const Normal3D &nA, const Normal3D &nB, const Normal3D &nC,
	float* const alpha2, float* const alpha1, float* const alpha0,
	float* const beta2, float* const beta1, float* const beta0,
	float* const denom2, float* const denom1, float* const denom0) {
	Vector3D e0, e1;
	rayDir.makeCoordinateSystem(&e0, &e1);

	Vector2D eAB, fAB;
	Vector2D eAC, fAC;
	Vector2D eAO, NA;
	{
		const Vector3D eABInObj = pB - pA;
		const Vector3D eACInObj = pC - pA;
		const Vector3D fABInObj = static_cast<Vector3D>(nB - nA);
		const Vector3D fACInObj = static_cast<Vector3D>(nC - nA);
		const Vector3D eAOInObj = rayOrg - pA;

		eAB = Vector2D(dot(eABInObj, e0), dot(eABInObj, e1));
		eAC = Vector2D(dot(eACInObj, e0), dot(eACInObj, e1));
		fAB = Vector2D(dot(fABInObj, e0), dot(fABInObj, e1));
		fAC = Vector2D(dot(fACInObj, e0), dot(fACInObj, e1));
		eAO = Vector2D(dot(eAOInObj, e0), dot(eAOInObj, e1));
		NA = Vector2D(dot(nA, e0), dot(nA, e1));
	}

	//*alpha2 = -NA.x * fAC.y + NA.y * fAC.x;
	//*alpha1 = eAO.x * fAC.y - eAC.y * NA.x - eAO.y * fAC.x + NA.y * eAC.x;
	//*alpha0 = eAO.x * eAC.y - eAO.y * eAC.x;
	//const float denA2 = fAB.x * fAC.y - fAB.y * fAC.x;
	//const float denA1 = eAB.x * fAC.y + fAB.x * eAC.y - eAB.y * fAC.x - fAB.y * eAC.x;
	//const float denA0 = eAB.x * eAC.y - eAB.y * eAC.x;
	//*beta2 = -NA.x * fAB.y + NA.y * fAB.x;
	//*beta1 = eAO.x * fAB.y - eAB.y * NA.x - eAO.y * fAB.x + NA.y * eAB.x;
	//*beta0 = eAO.x * eAB.y - eAO.y * eAB.x;
	//const float denB2 = fAC.x * fAB.y - fAC.y * fAB.x;
	//const float denB1 = eAC.x * fAB.y + fAC.x * eAB.y - eAC.y * fAB.x - fAC.y * eAB.x;
	//const float denB0 = eAC.x * eAB.y - eAC.y * eAB.x;

	// denA* == -denB* となるので分母はbeta*を反転すれば共通で使える。
	*denom2 = fAB.x * fAC.y - fAB.y * fAC.x;
	*denom1 = eAB.x * fAC.y + fAB.x * eAC.y - eAB.y * fAC.x - fAB.y * eAC.x;
	*denom0 = eAB.x * eAC.y - eAB.y * eAC.x;
	*alpha2 = -NA.x * fAC.y + NA.y * fAC.x;
	*alpha1 = eAO.x * fAC.y - eAC.y * NA.x - eAO.y * fAC.x + NA.y * eAC.x;
	*alpha0 = eAO.x * eAC.y - eAO.y * eAC.x;
	*beta2 = -(-NA.x * fAB.y + NA.y * fAB.x);
	*beta1 = -(eAO.x * fAB.y - eAB.y * NA.x - eAO.y * fAB.x + NA.y * eAB.x);
	*beta0 = -(eAO.x * eAB.y - eAO.y * eAB.x);
}

void testComputeCoeffs() {
	struct TestData {
		Point3D pA;
		Point3D pB;
		Point3D pC;
		Normal3D nA;
		Normal3D nB;
		Normal3D nC;
		Point2D tcA;
		Point2D tcB;
		Point2D tcC;

		Point3D SA(float h) const {
			return pA + h * nA;
		}
		Point3D SB(float h) const {
			return pB + h * nB;
		}
		Point3D SC(float h) const {
			return pC + h * nC;
		}
		Point3D p(const float alpha, const float beta) const {
			return (1 - alpha - beta) * pA + alpha * pB + beta * pC;
		}
		Normal3D n(const float alpha, const float beta) const {
			return (1 - alpha - beta) * nA + alpha * nB + beta * nC;
		}
		Point2D tc(const float alpha, const float beta) const {
			return (1 - alpha - beta) * tcA + alpha * tcB + beta * tcC;
		}
		Point3D S(const float alpha, const float beta, const float h) const {
			const Point3D ret = (1 - alpha - beta) * SA(h) + alpha * SB(h) + beta * SC(h);
			return ret;
		}
	};

	const TestData test = {
		Point3D(-0.5f, -0.4f, 0.1f),
		Point3D(0.4f, 0.1f, 0.4f),
		Point3D(-0.3f, 0.5f, 0.6f),
		normalize(Normal3D(-0.3f, -0.2f, 1.0f)),
		normalize(Normal3D(0.8f, -0.3f, 0.4f)),
		normalize(Normal3D(0.4f, 0.2f, 1.0f)),
		Point2D(0.4f, 0.7f),
		Point2D(0.2f, 0.2f),
		Point2D(0.7f, 0.4f)
	};

	vdb_frame();

	constexpr float axisScale = 1.0f;
	drawAxes(axisScale);

	constexpr bool showNegativeShell = true;

	const auto drawWiredDottedTriangle = []
	(const Point3D &pA, const Point3D pB, const Point3D &pC) {
		drawWiredTriangle(pA, pB, pC);
		setColor(RGB(0, 1, 1));
		drawPoint(pA);
		setColor(RGB(1, 0, 1));
		drawPoint(pB);
		setColor(RGB(1, 1, 0));
		drawPoint(pC);
	};

	// World-space Shell
	setColor(RGB(0.25f));
	drawWiredTriangle(test.pA, test.pB, test.pC);
	setColor(RGB(0.0f, 0.5f, 1.0f));
	drawVector(test.pA, test.nA, 1.0f);
	drawVector(test.pB, test.nB, 1.0f);
	drawVector(test.pC, test.nC, 1.0f);
	for (int i = 1; i <= 10; ++i) {
		const float p = static_cast<float>(i) / 10;
		setColor(RGB(p));
		drawWiredDottedTriangle(test.SA(p), test.SB(p), test.SC(p));
	}
	if constexpr (showNegativeShell) {
		for (int i = 1; i <= 10; ++i) {
			const float p = -static_cast<float>(i) / 10;
			setColor(RGB(-p));
			drawWiredDottedTriangle(test.SA(p), test.SB(p), test.SC(p));
		}
	}

	// World-space Ray
	const Point3D rayOrg(0.5f, -0.5f, 1.0f);
	const Vector3D rayDir = normalize(Vector3D(-0.7f, 1.3f, -0.5f));
	constexpr float rayLength = 2.0f;
	setColor(RGB(1.0f));
	drawCross(rayOrg, 0.05f);
	drawVector(rayOrg, rayDir, rayLength);

	// JP: 単一間区間の終わりと多重解区間の終わり
	drawCross(rayOrg + 348.0f / 500.0f * rayLength * rayDir, 0.05f);
	drawCross(rayOrg + 375.0f / 500.0f * rayLength * rayDir, 0.05f);

	constexpr Vector3D globalOffsetForCanonical(-1.0f, -2.0f, 0);
	constexpr Vector3D globalOffsetForTexture(1.0f, -2.0f, 0);
	drawAxes(axisScale, globalOffsetForCanonical);
	drawAxes(axisScale, globalOffsetForTexture);

	// Canonical-space and Texture-space Shell
	setColor(RGB(0.25f));
	drawWiredTriangle(
		globalOffsetForCanonical + Point3D(0, 0, 0),
		globalOffsetForCanonical + Point3D(1, 0, 0),
		globalOffsetForCanonical + Point3D(0, 1, 0));
	setColor(RGB(0.25f));
	drawWiredTriangle(
		globalOffsetForTexture + Point3D(test.tcA, 0.0f),
		globalOffsetForTexture + Point3D(test.tcB, 0.0f),
		globalOffsetForTexture + Point3D(test.tcC, 0.0f));
	setColor(RGB(0.0f, 0.5f, 1.0f));
	drawVector(globalOffsetForCanonical + Point3D(0, 0, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForCanonical + Point3D(1, 0, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForCanonical + Point3D(0, 1, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForTexture + Point3D(test.tcA, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForTexture + Point3D(test.tcB, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForTexture + Point3D(test.tcC, 0), Normal3D(0, 0, 1), 1.0f);
	for (int i = 1; i <= 10; ++i) {
		const float p = static_cast<float>(i) / 10;
		setColor(RGB(p));
		drawWiredDottedTriangle(
			globalOffsetForCanonical + Point3D(0, 0, 0) + p * Normal3D(0, 0, 1),
			globalOffsetForCanonical + Point3D(1, 0, 0) + p * Normal3D(0, 0, 1),
			globalOffsetForCanonical + Point3D(0, 1, 0) + p * Normal3D(0, 0, 1));
		setColor(RGB(p));
		drawWiredDottedTriangle(
			globalOffsetForTexture + Point3D(test.tcA, 0) + p * Normal3D(0, 0, 1),
			globalOffsetForTexture + Point3D(test.tcB, 0) + p * Normal3D(0, 0, 1),
			globalOffsetForTexture + Point3D(test.tcC, 0) + p * Normal3D(0, 0, 1));
	}
	if constexpr (showNegativeShell) {
		for (int i = 1; i <= 10; ++i) {
			const float p = -static_cast<float>(i) / 10;
			setColor(RGB(-p));
			drawWiredDottedTriangle(
				globalOffsetForCanonical + Point3D(0, 0, 0) + p * Normal3D(0, 0, 1),
				globalOffsetForCanonical + Point3D(1, 0, 0) + p * Normal3D(0, 0, 1),
				globalOffsetForCanonical + Point3D(0, 1, 0) + p * Normal3D(0, 0, 1));
			setColor(RGB(-p));
			drawWiredDottedTriangle(
				globalOffsetForTexture + Point3D(test.tcA, 0) + p * Normal3D(0, 0, 1),
				globalOffsetForTexture + Point3D(test.tcB, 0) + p * Normal3D(0, 0, 1),
				globalOffsetForTexture + Point3D(test.tcC, 0) + p * Normal3D(0, 0, 1));
		}
	}

	float alpha2, alpha1, alpha0;
	float beta2, beta1, beta0;
	float denom2, denom1, denom0;
	computeCoeffs(
		rayOrg, rayDir,
		test.pA, test.pB, test.pC,
		test.nA, test.nB, test.nC,
		&alpha2, &alpha1, &alpha0,
		&beta2, &beta1, &beta0,
		&denom2, &denom1, &denom0);

	const auto selectH = [](const float hs[3]) {
		float ret = hs[0];
		if (!std::isfinite(ret) || std::fabs(hs[1]) < std::fabs(ret))
			ret = hs[1];
		if (!std::isfinite(ret) || std::fabs(hs[2]) < std::fabs(ret))
			ret = hs[2];
		return ret;
	};

	// Canonical-space and Texture-space Ray
	Point3D prevCurvedRayPInCanonical;
	Point3D prevCurvedRayPInTexture;
	{
		float hs[3];
		findHeight(
			test.pA, test.pB, test.pC,
			test.nA, test.nB, test.nC,
			rayOrg,
			&hs[0], &hs[1], &hs[2]);
		const float h = selectH(hs);
		const float h2 = pow2(h);
		const float denom = denom2 * h2 + denom1 * h + denom0;
		prevCurvedRayPInCanonical = Point3D(
			(alpha2 * h2 + alpha1 * h + alpha0) / denom,
			(beta2 * h2 + beta1 * h + beta0) / denom,
			h);
		const Point2D tc2 =
			(denom2 - alpha2 - beta2) * test.tcA
			+ alpha2 * test.tcB
			+ beta2 * test.tcC;
		const Point2D tc1 =
			(denom1 - alpha1 - beta1) * test.tcA
			+ alpha1 * test.tcB
			+ beta1 * test.tcC;
		const Point2D tc0 =
			(denom0 - alpha0 - beta0) * test.tcA
			+ alpha0 * test.tcB
			+ beta0 * test.tcC;
		prevCurvedRayPInTexture = Point3D(Point2D(tc2 * h2 + tc1 * h + tc0) / denom, h);
	}
	setColor(RGB(1.0f));
	drawCross(globalOffsetForCanonical + prevCurvedRayPInCanonical, 0.05f);
	drawCross(globalOffsetForTexture + prevCurvedRayPInTexture, 0.05f);
	for (int i = 1; i <= 500; ++i) {
		const float t = static_cast<float>(i) / 500;
		float hs[3];
		hpprintf("%u:\n", i);
		findHeight(
			test.pA, test.pB, test.pC,
			test.nA, test.nB, test.nC,
			rayOrg + t * rayLength * rayDir,
			&hs[0], &hs[1], &hs[2]);
		const float h = selectH(hs);
		const float h2 = pow2(h);
		const float denom = denom2 * h2 + denom1 * h + denom0;
		const Point3D p(
			(alpha2 * h2 + alpha1 * h + alpha0) / denom,
			(beta2 * h2 + beta1 * h + beta0) / denom,
			h);
		const Point2D tc2 =
			(denom2 - alpha2 - beta2) * test.tcA
			+ alpha2 * test.tcB
			+ beta2 * test.tcC;
		const Point2D tc1 =
			(denom1 - alpha1 - beta1) * test.tcA
			+ alpha1 * test.tcB
			+ beta1 * test.tcC;
		const Point2D tc0 =
			(denom0 - alpha0 - beta0) * test.tcA
			+ alpha0 * test.tcB
			+ beta0 * test.tcC;
		Point3D tcp(Point2D(tc2 * h2 + tc1 * h + tc0) / denom, h);

		drawLine(globalOffsetForCanonical + prevCurvedRayPInCanonical, globalOffsetForCanonical + p);
		drawLine(globalOffsetForTexture + prevCurvedRayPInTexture, globalOffsetForTexture + tcp);
		prevCurvedRayPInCanonical = p;
		prevCurvedRayPInTexture = tcp;
		if (i == 374)
			printf("");
		if (i == 375)
			printf("");
	}
}



#define INPUT_TEXTURE_SPACE_MICRO_TRIANGLE 1

bool testNonlinearRayVsMicroTriangle(
	const Point3D &rayOrg, const Vector3D &rayDir,
	const Point3D &pA, const Point3D &pB, const Point3D &pC,
	const Normal3D &nA, const Normal3D &nB, const Normal3D &nC,
	const Point2D &tcA, const Point2D &tcB, const Point2D &tcC,
#if INPUT_TEXTURE_SPACE_MICRO_TRIANGLE
	const Point3D &mpAInTc, const Point3D &mpBInTc, const Point3D &mpCInTc,
#else
	const Point3D &mpAInCan, const Point3D &mpBInCan, const Point3D &mpCInCan,
#endif
	Point3D* hitPointInCan, Point3D* hitPointInTc, float* hitDist, Normal3D* hitNormalInObj) {
#if INPUT_TEXTURE_SPACE_MICRO_TRIANGLE
	// JP: テクスチャー空間中のマイクロ三角形を含む平面の方程式の係数を求める。
	const Normal3D nInTc(normalize(cross(mpBInTc - mpAInTc, mpCInTc - mpAInTc)));
	const float KInTc = dot(nInTc, -static_cast<Vector3D>(mpAInTc));
#endif

	// JP: 正準空間中のマイクロ三角形を含む平面の方程式の係数を求める。
#if INPUT_TEXTURE_SPACE_MICRO_TRIANGLE
	const Normal3D nInCan(
		nInTc.x * (tcB.x - tcA.x) + nInTc.y * (tcB.y - tcA.y),
		nInTc.x * (tcC.x - tcA.x) + nInTc.y * (tcC.y - tcA.y),
		nInTc.z);
	const float KInCan = nInTc.x * tcA.x + nInTc.y * tcA.y + KInTc;
#else
	const Normal3D nInCan(normalize(cross(mpBInCan - mpAInCan, mpCInCan - mpAInCan)));
	const float KInCan = dot(nInCan, -static_cast<Vector3D>(mpAInCan));
#endif

	// JP: 正準空間中のレイとマイクロ三角形を含む平面の交差判定。
	float h;
	{
		float alpha2, alpha1, alpha0;
		float beta2, beta1, beta0;
		float denom2, denom1, denom0;
		computeCoeffs(
			rayOrg, rayDir,
			pA, pB, pC,
			nA, nB, nC,
			&alpha2, &alpha1, &alpha0,
			&beta2, &beta1, &beta0,
			&denom2, &denom1, &denom0);

		float hs[3];
		const float a = nInCan.z * denom2;
		const float b = nInCan.x * alpha2 + nInCan.y * beta2 + nInCan.z * denom1 + KInCan * denom2;
		const float c = nInCan.x * alpha1 + nInCan.y * beta1 + nInCan.z * denom0 + KInCan * denom1;
		const float d = nInCan.x * alpha0 + nInCan.y * beta0 + KInCan * denom0;
		solveCubicEquation(a, b, c, d, &hs[0], &hs[1], &hs[2]);
		h = hs[0];
		if (h < 0 || h > 1.0f)
			h = hs[1];
		if (h < 0 || h > 1.0f)
			h = hs[2];
	}
	if (h < 0 || h > 1.0f) {
		*hitDist = INFINITY;
		return false;
	}

	const Point3D SAh = pA + h * nA;
	const Point3D SBh = pB + h * nB;
	const Point3D SCh = pC + h * nC;

	// JP: 正準空間の他の座標を求める。
	float alpha, beta;
	Matrix3x3 transposedAdjMat;
	{
		const Vector3D eSABInObj = SBh - SAh;
		const Vector3D eSACInObj = SCh - SAh;
		const Vector3D eSAOInObj = rayOrg - SAh;

		Vector3D e0, e1;
		rayDir.makeCoordinateSystem(&e0, &e1);
		const Vector2D eSAB(dot(eSABInObj, e0), dot(eSABInObj, e1));
		const Vector2D eSAC(dot(eSACInObj, e0), dot(eSACInObj, e1));
		const Vector2D eSAO(dot(eSAOInObj, e0), dot(eSAOInObj, e1));

		const float det0 = eSAB.x * eSAC.y - eSAC.x * eSAB.y;
		float curDet = det0;
		Matrix2x2 lhsMat(eSAB, eSAC);
		Vector2D rhsConsts = eSAO;
		const float det1 = eSAB.x * nInCan.y - eSAC.x * nInCan.x;
		if (std::fabs(det1) > std::fabs(curDet)) {
			lhsMat = Matrix2x2(Vector2D(eSAB.x, nInCan.x), Vector2D(eSAC.x, nInCan.y));
			rhsConsts = Vector2D(eSAO.x, -nInCan.z * h - KInCan);
			curDet = det1;
		}
		const float det2 = eSAB.y * nInCan.y - eSAC.y * nInCan.x;
		if (std::fabs(det2) > std::fabs(curDet)) {
			lhsMat = Matrix2x2(Vector2D(eSAB.y, nInCan.x), Vector2D(eSAC.y, nInCan.y));
			rhsConsts = Vector2D(eSAO.y, -nInCan.z * h - KInCan);
			curDet = det2;
		}

		const float recDet = 1.0f / curDet;
		alpha = recDet * (lhsMat[1][1] * rhsConsts.x - lhsMat[1][0] * rhsConsts.y);
		beta = recDet * (-lhsMat[0][1] * rhsConsts.x + lhsMat[0][0] * rhsConsts.y);

		// JP: 論文中の余因子行列は「ij成分がij余因子である行列」を指しているが、
		//     このコードではadjugate()は「ij成分がij余因子である行列の転置行列」を指す。
		transposedAdjMat = adjugateWithoutTranspose(Matrix3x3(
			eSABInObj,
			eSACInObj,
			static_cast<Vector3D>((1 - alpha - beta) * nA + alpha * nB + beta * nC)));
	}
	if (alpha < 0.0f || alpha > 1.0f ||
		beta < 0.0f || beta > 1.0f ||
		(alpha + beta) > 1.0f) {
		*hitDist = INFINITY;
		return false;
	}

	*hitPointInCan = Point3D(alpha, beta, h);
	*hitPointInTc = Point3D((1 - alpha - beta) * tcA + alpha * tcB + beta * tcC, h);
	{
#if INPUT_TEXTURE_SPACE_MICRO_TRIANGLE
		const Vector2D eAB = mpBInTc.xy() - mpAInTc.xy();
		const Vector2D eBC = mpCInTc.xy() - mpBInTc.xy();
		const Vector2D eCA = mpAInTc.xy() - mpCInTc.xy();
		const Vector2D eAP = hitPointInTc->xy() - mpAInTc.xy();
		const Vector2D eBP = hitPointInTc->xy() - mpBInTc.xy();
		const Vector2D eCP = hitPointInTc->xy() - mpCInTc.xy();
#else
		const Vector2D eAB = mpBInCan.xy() - mpAInCan.xy();
		const Vector2D eBC = mpCInCan.xy() - mpBInCan.xy();
		const Vector2D eCA = mpAInCan.xy() - mpCInCan.xy();
		const Vector2D eAP = hitPointInCan->xy() - mpAInCan.xy();
		const Vector2D eBP = hitPointInCan->xy() - mpBInCan.xy();
		const Vector2D eCP = hitPointInCan->xy() - mpCInCan.xy();
#endif
		const float cAB = cross(eAB, eAP);
		const float cBC = cross(eBC, eBP);
		const float cCA = cross(eCA, eCP);
		if ((cAB < 0 || cBC < 0 || cCA < 0) && (cAB >= 0 || cBC >= 0 || cCA >= 0)) {
			*hitDist = INFINITY;
			return false;
		}
	}

	*hitDist = dot(
		rayDir,
		(1 - alpha - beta) * SAh + alpha * SBh + beta * SCh - rayOrg);

	*hitNormalInObj = normalize(transposedAdjMat * nInCan);

	return true;
}

void testNonlinearRayVsMicroTriangle() {
	struct TestData {
		Point3D pA;
		Point3D pB;
		Point3D pC;
		Normal3D nA;
		Normal3D nB;
		Normal3D nC;
		Point2D tcA;
		Point2D tcB;
		Point2D tcC;

		Point3D microTris[3]; // Canonical coordinates

		Point3D SA(float h) const {
			return pA + h * nA;
		}
		Point3D SB(float h) const {
			return pB + h * nB;
		}
		Point3D SC(float h) const {
			return pC + h * nC;
		}
		Point3D p(const float alpha, const float beta) const {
			return (1 - alpha - beta) * pA + alpha * pB + beta * pC;
		}
		Normal3D n(const float alpha, const float beta) const {
			return (1 - alpha - beta) * nA + alpha * nB + beta * nC;
		}
		Point2D tc(const float alpha, const float beta) const {
			return (1 - alpha - beta) * tcA + alpha * tcB + beta * tcC;
		}
		Point3D S(const float alpha, const float beta, const float h) const {
			const Point3D ret = (1 - alpha - beta) * SA(h) + alpha * SB(h) + beta * SC(h);
			return ret;
		}
	};

	const TestData test = {
		Point3D(-0.5f, -0.4f, 0.1f),
		Point3D(0.4f, 0.1f, 0.4f),
		Point3D(-0.3f, 0.5f, 0.6f),
		normalize(Normal3D(-0.3f, -0.2f, 1.0f)),
		normalize(Normal3D(0.8f, -0.3f, 0.4f)),
		normalize(Normal3D(0.4f, 0.2f, 1.0f)),
		Point2D(0.4f, 0.7f),
		Point2D(0.2f, 0.2f),
		Point2D(0.7f, 0.4f),
		{
			Point3D(0.2f, 0.1f, 0.4f),
			Point3D(0.6f, 0.3f, 0.1f),
			Point3D(0.2f, 0.8f, 0.5f),
		}
	};

	const float mAlphaA = test.microTris[0].x;
	const float mBetaA = test.microTris[0].y;
	const float mhA = test.microTris[0].z;
	const float mAlphaB = test.microTris[1].x;
	const float mBetaB = test.microTris[1].y;
	const float mhB = test.microTris[1].z;
	const float mAlphaC = test.microTris[2].x;
	const float mBetaC = test.microTris[2].y;
	const float mhC = test.microTris[2].z;

	const Point3D mpAInCan(mAlphaA, mBetaA, mhA);
	const Point3D mpBInCan(mAlphaB, mBetaB, mhB);
	const Point3D mpCInCan(mAlphaC, mBetaC, mhC);
	const Point3D mpAInTc(test.tc(mAlphaA, mBetaA), mhA);
	const Point3D mpBInTc(test.tc(mAlphaB, mBetaB), mhB);
	const Point3D mpCInTc(test.tc(mAlphaC, mBetaC), mhC);

	const Point3D rayOrg(0.5f, -0.5f, 1.0f);
	const Vector3D rayDir = normalize(Vector3D(-0.7f, 1.3f, -0.5f));
	constexpr float rayLength = 1.5f;

	Point3D hitPointInCan;
	Point3D hitPointInTc;
	float hitDist;
	Normal3D hitNormalInObj;
	const bool hit = testNonlinearRayVsMicroTriangle(
		rayOrg, rayDir,
		test.pA, test.pB, test.pC,
		test.nA, test.nB, test.nC,
		test.tcA, test.tcB, test.tcC,
#if INPUT_TEXTURE_SPACE_MICRO_TRIANGLE
		mpAInTc, mpBInTc, mpCInTc,
#else
		mpAInCan, mpBInCan, mpCInCan,
#endif
		&hitPointInCan, &hitPointInTc, &hitDist, &hitNormalInObj);

	vdb_frame();

	constexpr float axisScale = 1.0f;
	drawAxes(axisScale);

	constexpr bool showNegativeShell = false;

	const auto drawWiredDottedTriangle = []
	(const Point3D &pA, const Point3D pB, const Point3D &pC) {
		drawWiredTriangle(pA, pB, pC);
		setColor(RGB(0, 1, 1));
		drawPoint(pA);
		setColor(RGB(1, 0, 1));
		drawPoint(pB);
		setColor(RGB(1, 1, 0));
		drawPoint(pC);
	};

	// World-space Shell
	setColor(RGB(0.25f));
	drawWiredTriangle(test.pA, test.pB, test.pC);
	setColor(RGB(0.0f, 0.5f, 1.0f));
	drawVector(test.pA, test.nA, 1.0f);
	drawVector(test.pB, test.nB, 1.0f);
	drawVector(test.pC, test.nC, 1.0f);
	for (int i = 1; i <= 10; ++i) {
		const float p = static_cast<float>(i) / 10;
		setColor(RGB(p));
		drawWiredDottedTriangle(test.SA(p), test.SB(p), test.SC(p));
	}
	if constexpr (showNegativeShell) {
		for (int i = 1; i <= 10; ++i) {
			const float p = -static_cast<float>(i) / 10;
			setColor(RGB(-p));
			drawWiredDottedTriangle(test.SA(p), test.SB(p), test.SC(p));
		}
	}

	// World-space Ray
	setColor(RGB(1.0f));
	drawCross(rayOrg, 0.05f);
	drawVector(rayOrg, rayDir, rayLength);
	if (hit) {
		setColor(RGB(1, 0.5f, 0));
		const Point3D hpA = test.S(hitPointInCan.x, hitPointInCan.y, hitPointInCan.z);
		const Point3D hpB = rayOrg + hitDist * rayDir;
		drawCross(hpA, 0.05f);
		drawCross(hpB, 0.05f);
		setColor(RGB(0, 1, 1));
		drawVector(hpA, hitNormalInObj, 0.1f);
	}

	// World-space Micro-Triangle
	Point3D prevSAToB;
	Point3D prevSBToC;
	Point3D prevSCToA;
	{
		const float p = 0.0f;

		const float mAlphaAToB = lerp(mAlphaA, mAlphaB, p);
		const float mBetaAToB = lerp(mBetaA, mBetaB, p);
		const float mhAToB = lerp(mhA, mhB, p);
		prevSAToB = test.S(mAlphaAToB, mBetaAToB, mhAToB);

		const float mAlphaBToC = lerp(mAlphaB, mAlphaC, p);
		const float mBetaBToC = lerp(mBetaB, mBetaC, p);
		const float mhBToC = lerp(mhB, mhC, p);
		prevSBToC = test.S(mAlphaBToC, mBetaBToC, mhBToC);

		const float mAlphaCToA = lerp(mAlphaC, mAlphaA, p);
		const float mBetaCToA = lerp(mBetaC, mBetaA, p);
		const float mhCToA = lerp(mhC, mhA, p);
		prevSCToA = test.S(mAlphaCToA, mBetaCToA, mhCToA);

		setColor(RGB(0, 1, 1));
		drawPoint(prevSAToB);
		setColor(RGB(1, 0, 1));
		drawPoint(prevSBToC);
		setColor(RGB(1, 1, 0));
		drawPoint(prevSCToA);
	}
	setColor(RGB(1.0f));
	for (int i = 1; i <= 100; ++i) {
		const float p = static_cast<float>(i) / 100;

		const float mAlphaAToB = lerp(mAlphaA, mAlphaB, p);
		const float mBetaAToB = lerp(mBetaA, mBetaB, p);
		const float mhAToB = lerp(mhA, mhB, p);
		const Point3D SAToB = test.S(mAlphaAToB, mBetaAToB, mhAToB);
		drawLine(prevSAToB, SAToB);
		prevSAToB = SAToB;

		const float mAlphaBToC = lerp(mAlphaB, mAlphaC, p);
		const float mBetaBToC = lerp(mBetaB, mBetaC, p);
		const float mhBToC = lerp(mhB, mhC, p);
		const Point3D SBToC = test.S(mAlphaBToC, mBetaBToC, mhBToC);
		drawLine(prevSBToC, SBToC);
		prevSBToC = SBToC;

		const float mAlphaCToA = lerp(mAlphaC, mAlphaA, p);
		const float mBetaCToA = lerp(mBetaC, mBetaA, p);
		const float mhCToA = lerp(mhC, mhA, p);
		const Point3D SCToA = test.S(mAlphaCToA, mBetaCToA, mhCToA);
		drawLine(prevSCToA, SCToA);
		prevSCToA = SCToA;
	}

	constexpr Vector3D globalOffsetForCanonical(-1.0f, -2.0f, 0);
	constexpr Vector3D globalOffsetForTexture(1.0f, -2.0f, 0);
	drawAxes(axisScale, globalOffsetForCanonical);
	drawAxes(axisScale, globalOffsetForTexture);

	// Canonical-space and Texture-space Shell
	setColor(RGB(0.25f));
	drawWiredTriangle(
		globalOffsetForCanonical + Point3D(0, 0, 0),
		globalOffsetForCanonical + Point3D(1, 0, 0),
		globalOffsetForCanonical + Point3D(0, 1, 0));
	setColor(RGB(0.25f));
	drawWiredTriangle(
		globalOffsetForTexture + Point3D(test.tcA, 0.0f),
		globalOffsetForTexture + Point3D(test.tcB, 0.0f),
		globalOffsetForTexture + Point3D(test.tcC, 0.0f));
	setColor(RGB(0.0f, 0.5f, 1.0f));
	drawVector(globalOffsetForCanonical + Point3D(0, 0, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForCanonical + Point3D(1, 0, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForCanonical + Point3D(0, 1, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForTexture + Point3D(test.tcA, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForTexture + Point3D(test.tcB, 0), Normal3D(0, 0, 1), 1.0f);
	drawVector(globalOffsetForTexture + Point3D(test.tcC, 0), Normal3D(0, 0, 1), 1.0f);
	for (int i = 1; i <= 10; ++i) {
		const float p = static_cast<float>(i) / 10;
		setColor(RGB(p));
		drawWiredDottedTriangle(
			globalOffsetForCanonical + Point3D(0, 0, 0) + p * Normal3D(0, 0, 1),
			globalOffsetForCanonical + Point3D(1, 0, 0) + p * Normal3D(0, 0, 1),
			globalOffsetForCanonical + Point3D(0, 1, 0) + p * Normal3D(0, 0, 1));
		setColor(RGB(p));
		drawWiredDottedTriangle(
			globalOffsetForTexture + Point3D(test.tcA, 0) + p * Normal3D(0, 0, 1),
			globalOffsetForTexture + Point3D(test.tcB, 0) + p * Normal3D(0, 0, 1),
			globalOffsetForTexture + Point3D(test.tcC, 0) + p * Normal3D(0, 0, 1));
	}
	if constexpr (showNegativeShell) {
		for (int i = 1; i <= 10; ++i) {
			const float p = -static_cast<float>(i) / 10;
			setColor(RGB(-p));
			drawWiredDottedTriangle(
				globalOffsetForCanonical + Point3D(0, 0, 0) + p * Normal3D(0, 0, 1),
				globalOffsetForCanonical + Point3D(1, 0, 0) + p * Normal3D(0, 0, 1),
				globalOffsetForCanonical + Point3D(0, 1, 0) + p * Normal3D(0, 0, 1));
			setColor(RGB(-p));
			drawWiredDottedTriangle(
				globalOffsetForTexture + Point3D(test.tcA, 0) + p * Normal3D(0, 0, 1),
				globalOffsetForTexture + Point3D(test.tcB, 0) + p * Normal3D(0, 0, 1),
				globalOffsetForTexture + Point3D(test.tcC, 0) + p * Normal3D(0, 0, 1));
		}
	}

	float alpha2, alpha1, alpha0;
	float beta2, beta1, beta0;
	float denom2, denom1, denom0;
	computeCoeffs(
		rayOrg, rayDir,
		test.pA, test.pB, test.pC,
		test.nA, test.nB, test.nC,
		&alpha2, &alpha1, &alpha0,
		&beta2, &beta1, &beta0,
		&denom2, &denom1, &denom0);

	const auto computeTcCoeffs = []
	(const Point2D &tcA, const Point2D &tcB, const Point2D &tcC,
	 const float denom, const float alpha, const float beta) {
		return (denom - alpha - beta) * tcA + alpha * tcB + beta * tcC;
	};

	const auto selectH = [](const float hs[3]) {
		float ret = hs[0];
		if (!std::isfinite(ret) || std::fabs(hs[1]) < std::fabs(ret))
			ret = hs[1];
		if (!std::isfinite(ret) || std::fabs(hs[2]) < std::fabs(ret))
			ret = hs[2];
		return ret;
	};

	// Canonical-space and Texture-space Ray
	Point3D prevCurvedRayPInCanonical;
	Point3D prevCurvedRayPInTexture;
	{
		float hs[3];
		findHeight(
			test.pA, test.pB, test.pC,
			test.nA, test.nB, test.nC,
			rayOrg,
			&hs[0], &hs[1], &hs[2]);
		const float h = selectH(hs);
		const float h2 = pow2(h);
		const float denom = denom2 * h2 + denom1 * h + denom0;
		prevCurvedRayPInCanonical = Point3D(
			(alpha2 * h2 + alpha1 * h + alpha0) / denom,
			(beta2 * h2 + beta1 * h + beta0) / denom,
			h);
		const Point2D tc2 = computeTcCoeffs(test.tcA, test.tcB, test.tcC, denom2, alpha2, beta2);
		const Point2D tc1 = computeTcCoeffs(test.tcA, test.tcB, test.tcC, denom1, alpha1, beta1);
		const Point2D tc0 = computeTcCoeffs(test.tcA, test.tcB, test.tcC, denom0, alpha0, beta0);
		prevCurvedRayPInTexture = Point3D(Point2D(tc2 * h2 + tc1 * h + tc0) / denom, h);
	}
	setColor(RGB(1.0f));
	drawCross(globalOffsetForCanonical + prevCurvedRayPInCanonical, 0.05f);
	drawCross(globalOffsetForTexture + prevCurvedRayPInTexture, 0.05f);
	for (int i = 1; i <= 500; ++i) {
		const float t = static_cast<float>(i) / 500;
		float hs[3];
		hpprintf("%u:\n", i);
		findHeight(
			test.pA, test.pB, test.pC,
			test.nA, test.nB, test.nC,
			rayOrg + t * rayLength * rayDir,
			&hs[0], &hs[1], &hs[2]);
		const float h = selectH(hs);
		const float h2 = pow2(h);
		const float denom = denom2 * h2 + denom1 * h + denom0;
		const Point3D p(
			(alpha2 * h2 + alpha1 * h + alpha0) / denom,
			(beta2 * h2 + beta1 * h + beta0) / denom,
			h);
		const Point2D tc2 = computeTcCoeffs(test.tcA, test.tcB, test.tcC, denom2, alpha2, beta2);
		const Point2D tc1 = computeTcCoeffs(test.tcA, test.tcB, test.tcC, denom1, alpha1, beta1);
		const Point2D tc0 = computeTcCoeffs(test.tcA, test.tcB, test.tcC, denom0, alpha0, beta0);
		const Point3D tcp(Point2D(tc2 * h2 + tc1 * h + tc0) / denom, h);

		drawLine(globalOffsetForCanonical + prevCurvedRayPInCanonical, globalOffsetForCanonical + p);
		drawLine(globalOffsetForTexture + prevCurvedRayPInTexture, globalOffsetForTexture + tcp);
		prevCurvedRayPInCanonical = p;
		prevCurvedRayPInTexture = tcp;
	}
	if (hit) {
		setColor(RGB(1, 0.5f, 0));
		drawCross(globalOffsetForCanonical + hitPointInCan, 0.05f);
		drawCross(globalOffsetForTexture + hitPointInTc, 0.05f);
	}

	// Canonical-space and Texture-space Micro-Triangle
	{
		setColor(RGB(1.0f));
		drawWiredDottedTriangle(
			globalOffsetForCanonical + mpAInCan,
			globalOffsetForCanonical + mpBInCan,
			globalOffsetForCanonical + mpCInCan);

		setColor(RGB(1.0f));
		drawWiredDottedTriangle(
			globalOffsetForTexture + mpAInTc,
			globalOffsetForTexture + mpBInTc,
			globalOffsetForTexture + mpCInTc);
	}
}

#endif