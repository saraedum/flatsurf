/**********************************************************************
 *  This file is part of flatsurf.
 *
 *        Copyright (C) 2018 Alex Eskin
 *        Copyright (C) 2019 Vincent Delecroix
 *        Copyright (C) 2019 Julian Rüth
 *
 *  Flatsurf is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Flatsurf is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with flatsurf. If not, see <https://www.gnu.org/licenses/>.
 *********************************************************************/

#include <boost/math/constants/constants.hpp>
#include <cassert>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "./elementary_geometry.h"
#include "./globals.h"
#include "./number_field.h"
#include "./oedge.h"
#include "./params.h"
#include "./two_complex.h"
#include "./uedge.h"

using namespace polygon;
using boost::math::constants::pi;
using std::cout;
using std::endl;
using std::list;
using std::string;
using std::vector;

void TwoComplex::InitCyclotomicField(int n) {
  /*use the cyclotomic field generated by the primitive n'th roots of unity */

  /*
      complex<COORD> nth_rootofunity(cos((COORD)(2*MY_PI/n)),
     sin((COORD)(2*MY_PI/n))); ratpoly cyc = cyclotomic_poly(n); vector<bigrat>
     coeffs = cyc.coefficients; (NumberField<bigrat>::F) = new
     NumberField(&coeffs[0], cyc.degree(), nth_rootofunity);
      F->store_conjugate(algebraicQ(1, F).pow(n-1));

      //F->print_mt();
      */

  NumberField<bigrat>::F = InitCyclotomic<bigrat>(n);
  NumberField<int64_t>::F = InitCyclotomic<int64_t>(n);
}

void TwoComplex::BuildTriangle(int alpha, int beta, int gamma) {
  list<OEdge> tmp_oedge_list;
  UEdge* ue;
  OEdge* oe;

  int denom;

  denom = alpha + beta + gamma;

  InitCyclotomicField(denom); /*F is cyclotomic field */
  algebraicQ zeta =
      algebraicQ(1, NumberField<bigrat>::F); /* zeta = exp(2*pi*i/denom) */

  algebraicQ one_F =
      algebraicQ(0ul, NumberField<bigrat>::F); /* this is 1 in F */
  cout << "zeta = " << zeta << " one_F = " << one_F << endl;
  cout << "zeta.cx = " << zeta.tocomplex()
       << " one_F.cx = " << one_F.tocomplex() << endl;

  NumberField<bigrat>::F->print_mt();

  algebraicQ q_alg =
      (one_F - zeta.pow(alpha + gamma)) / (one_F - zeta.pow(gamma));
  algebraicQ p_alg = one_F;

  algebraicQ num = one_F - zeta.pow(alpha + gamma);

  cout << "gamma = " << gamma << " zeta.pow(gamma) = " << zeta.pow(gamma)
       << endl;

  algebraicQ den = one_F - zeta.pow(gamma);

  cout << "num = " << num << " den = " << denom << endl;

  cout << "q_alg = " << q_alg << " q_alg.cx = " << q_alg.tocomplex();

  COORD a, b;

  a = pi<COORD>() * alpha / denom;
  b = pi<COORD>() * beta / denom;

  //    COORD c = MY_PI - a - b;

  //    std::cout << "check a: " << cosl(a)*cosl(a) + sinl(a)*sinl(a) - 1 <<
  //    endl; std::cout << "check b: " << cos(b)*cos(b) + sin(b)*sin(b) - 1 <<
  //    endl; std::cout << "check c: " << cos(c)*cos(c) + sin(c)*sin(c) - 1 <<
  //    endl;

  //    std::cout << "check PI: " << sinl(MY_PI) << endl;

  Point p = Point(1 / tan(a) + 1 / tan(b), 0);
  Point q = Point(1 / tan(a), 1.0);

  COORD factor = 1 / tan(a) + 1 / tan(b);

  p /= factor;
  q /= factor;

  std::cout << "Checking " << q_alg.tocomplex() - q << " "
            << p_alg.tocomplex() - p;

  //   std::cout << "p = " << abs(p) << " q = " << abs(q) << " p-q = " <<
  //   abs(p-q)
  //	 << endl;

  //    std::cout << "check 1: " << abs(q-p)/sin(a) - abs(q)/sin(b) << endl;
  //    std::cout << "check 2: " << abs(q-p)/sin(a) - abs(p)/sin(c) << endl;

  //    std::cout << "check 3: " << abs(p)*abs(p)+abs(q)*abs(q)
  //          -2*abs(p)*abs(q)*cos(a)-abs(q-p)*abs(q-p) << endl;

  vector<algebraicQ> p_alg_v;
  p_alg_v.push_back(p_alg);

  vector<algebraicQ> q_alg_v;
  q_alg_v.push_back(q_alg);

  alg_tQ p_algt(p_alg_v);
  alg_tQ q_algt(q_alg_v);

  BigPointQ bp(p_algt);
  BigPointQ bq(q_algt);

  std::cout << "Checking " << bq.cx - q << " " << bp.cx - p << endl;

  Vertex* v0 = AddVertex(UNDEFINED);
  Vertex* v1 = AddVertex(UNDEFINED);
  Vertex* v2 = AddVertex(UNDEFINED);

  ue = AddUEdge(UNDEFINED, v0, v1, bp);
  oe = new OEdge(ue, 1);
  ue->label = 1;
  tmp_oedge_list.insert(tmp_oedge_list.end(), *oe);

  ue = AddUEdge(UNDEFINED, v1, v2, bq - bp);
  oe = new OEdge(ue, 1);
  ue->label = 2;
  tmp_oedge_list.insert(tmp_oedge_list.end(), *oe);

  ue = AddUEdge(UNDEFINED, v2, v0, -bq);
  oe = new OEdge(ue, 1);
  ue->label = 3;
  tmp_oedge_list.insert(tmp_oedge_list.end(), *oe);

  AddFace(UNDEFINED, tmp_oedge_list);
}

void MakeRealQuad(int int_angles[], Point& p, Point& q, Point& r) {
  /* Calculate the coordinates of a quadrilateral with angles
    adds a quadrilateral with angles  2*pi*a/(a+b+c+d), 2*pi*b/(a+b+c+d),
  2*pi*c/(a+b+c+d), 2*pi*d/(a+b+c+d), where a,b,c,d are the entries of
  int_angles. The result is a quadrilateral with vertices 0, p, q, and r. The
  angles at these vertices are in this order. The biggest angle is assumed to be
  2*pi*a/(a+b+c+d), which is the angle at 0. The angle at a is bisected into a1
  and a2, using the formula below. Also, p is always going to be (1,0).

   */

  int denom = int_angles[0] + int_angles[1] + int_angles[2] + int_angles[3];

  COORD a = 2 * pi<COORD>() * int_angles[0] / denom;
  COORD b = 2 * pi<COORD>() * int_angles[1] / denom;
  COORD c = 2 * pi<COORD>() * int_angles[2] / denom;
  COORD d = 2 * pi<COORD>() - a - b - c;

  std::cout << "a =  " << a << " b = " << b << " c= " << c << " d=" << d
            << endl;

  COORD a1_max = pi<COORD>() - b;
  COORD a2_max = pi<COORD>() - d;
  COORD a1_min = a - a2_max;

  std::cout << "a1_min = " << a1_min << " a1_max = " << a1_max << endl;

  COORD a1 = a1_min / pi<COORD>() + a1_max * (1.0 - 1 / pi<COORD>()); /*hack*/
  std::cout << "a1 = " << a1 << endl;
  COORD a2 = a - a1;

  COORD c1 = pi<COORD>() - a1 - b;
  COORD c2 = c - c1;
  /*
  std::cout << "check a: " << cos(a)*cos(a) + sin(a)*sin(a) - 1 << endl;
  std::cout << "check b: " << cos(b)*cos(b) + sin(b)*sin(b) - 1 << endl;
  std::cout << "check c: " << cos(c)*cos(c) + sin(c)*sin(c) - 1 << endl;

  std::cout << "check PI: " << sin(MY_PI) << endl;
  */

  p = Point(1 / tan(a1) + 1 / tan(b), 0);
  q = Point(1 / tan(a1), 1.0);

  COORD s = abs(q) * tan(c2) / (tan(a2) + tan(c2));
  COORD h = s * tan(a2);

  r = Point(cos(a1) * s - sin(a1) * h, sin(a1) * s + cos(a1) * h);

  COORD factor = 1 / tan(a1) + 1 / tan(b);
  p = p / factor;
  q = q / factor;
  r = r / factor;

  /*
      std::cout << "angle a1:" << true_angle(p,q) - a1 << endl;
      std::cout << "angle a2:" << angle(tmp1,tmp2) -a2 << endl;
      std::cout << "angle c2:" << angle(tmp1-tmp2, -tmp2) - c2 << endl;
  */
  std::cout << "angle a check: " << true_angle(p, r) - a << endl;
  std::cout << "angle b check: " << true_angle(q - p, -p) - b << endl;
  std::cout << "angle c check: " << true_angle(r - q, q - p) - c << endl;
  std::cout << "angle d check: " << true_angle(-r, q - r) - d << endl;

  std::cout << "p = " << abs(p) << " q = " << abs(q) << " p-q = " << abs(p - q)
            << endl;

  std::cout << "p = " << p << " q = " << q << " r = " << r << endl;

  //    std::cout << "check 1: " << abs(q-p)/sin(a) - abs(q)/sin(b) << endl;
  //    std::cout << "check 2: " << abs(q-p)/sin(a) - abs(p)/sin(c) << endl;

  //    std::cout << "check 3: " << abs(p)*abs(p)+abs(q)*abs(q)
  //          -2*abs(p)*abs(q)*cos(a)-abs(q-p)*abs(q-p) << endl;

  return;
}

void TwoComplex::BuildQuad(int alpha, int beta, int gamma, int delta) {
  list<OEdge> tmp_oedge_list;
  UEdge* ue;
  OEdge* oe;
  int denom = alpha + beta + gamma + delta;

  /* find biggest angle */

  int tmp[4] = {alpha, beta, gamma, delta};
  int max_angle = -1.0;
  int max_loc = -1;
  for (int i = 0; i < 4; i++) {
    if (tmp[i] > max_angle) {
      max_angle = tmp[i];
      max_loc = i;
    }
  }
  assert(max_loc >= 0);

  std::cout << "max loc = " << max_loc << endl;
  /* cyclically permute until a is the biggest */

  int tmp2[4];
  for (int j = 0; j < 4; j++) {
    int k = j - max_loc;
    if (k < 0) {
      k = k + 4;
    }
    tmp2[k] = tmp[j];
  }
  alpha = tmp2[0];
  beta = tmp2[1];
  gamma = tmp2[2];
  delta = tmp2[3];

  std::cout << "alpha =  " << alpha << " beta = " << beta
            << " gamma = " << gamma << " delta =" << delta << endl;

  Point p, q, r;

  MakeRealQuad(tmp2, p, q, r);

  InitCyclotomicField(denom); /*F is cyclotomic field */
  algebraicQ zeta =
      algebraicQ(1, NumberField<bigrat>::F); /* zeta = exp(2*pi*i/denom) */
  algebraicQ one_F =
      algebraicQ(0ul, NumberField<bigrat>::F); /* this is 1 in F */
  algebraicQ zero_F = algebraicQ(NumberField<bigrat>::F);

  Point t_cx = r * zeta.pow(-alpha).tocomplex();
  std::cout << "Imaginary of t_cx = " << t_cx.imag() << endl;

  COORD t = t_cx.real();  // match MakeRealQuad, r = t zeta^{alpha}

  Params::AddParams(1, &t);

  vector<algebraicQ> r_alg_v, p_alg_v;
  r_alg_v.push_back(zero_F);
  r_alg_v.push_back(zeta.pow(alpha));

  p_alg_v.push_back(one_F);
  p_alg_v.push_back(zero_F);

  //    algebraicQ r_alg[2] = {zero_F, zeta.pow(alpha)};
  //    algebraicQ p_alg[2] = {one_F, zero_F};

  alg_tQ p_algt(p_alg_v);
  alg_tQ r_algt(r_alg_v);

  std::cout << "p_algt = " << p_algt << endl;
  std::cout << "r_algt = " << r_algt << endl;

  algebraicQ r0_alg =
      (zeta.pow(-beta) - zeta.pow(beta)) / (zeta.pow(gamma) - zeta.pow(-gamma));
  algebraicQ r1_alg = (zeta.pow(alpha + beta) - zeta.pow(-alpha - beta)) /
                      (zeta.pow(gamma) - zeta.pow(-gamma));

  vector<algebraicQ> q_alg_v;
  q_alg_v.push_back(r0_alg * zeta.pow(alpha + delta));
  q_alg_v.push_back(zeta.pow(alpha) + r1_alg * zeta.pow(alpha + delta));

  //    algebraicQ q_alg[2] = { r0_alg*zeta.pow(alpha+delta),
  //			      zeta.pow(alpha) + r1_alg*zeta.pow(alpha+delta)};

  alg_tQ q_algt(q_alg_v);

  Point p_alt = p_alg_v[0].tocomplex() + t * p_alg_v[1].tocomplex();
  Point q_alt = q_alg_v[0].tocomplex() + t * q_alg_v[1].tocomplex();
  Point r_alt = r_alg_v[0].tocomplex() + t * r_alg_v[1].tocomplex();

  std::cout << "Checking p_alt " << p_alt - p << endl;
  std::cout << "Checking q_alt " << q_alt - q << endl;
  std::cout << "Checking r_alt " << r_alt - r << endl;

  BigPointQ bp(p_algt);
  BigPointQ bq(q_algt);
  BigPointQ br(r_algt);

  std::cout << "Checking p_algt " << bp.cx - p << endl;
  std::cout << "Checking q_algt " << bq.cx - q << endl;
  std::cout << "Checking r_algt " << br.cx - r << endl;

  Vertex* v0 = AddVertex(UNDEFINED);
  Vertex* v1 = AddVertex(UNDEFINED);
  Vertex* v2 = AddVertex(UNDEFINED);
  Vertex* v3 = AddVertex(UNDEFINED);

  ue = AddUEdge(UNDEFINED, v0, v1, bp);
  oe = new OEdge(ue, 1);
  ue->label = 1;
  tmp_oedge_list.insert(tmp_oedge_list.end(), *oe);
  delete oe;

  ue = AddUEdge(UNDEFINED, v1, v2, bq - bp);
  oe = new OEdge(ue, 1);
  ue->label = 2;
  tmp_oedge_list.insert(tmp_oedge_list.end(), *oe);
  delete oe;

  ue = AddUEdge(UNDEFINED, v2, v3, br - bq);
  oe = new OEdge(ue, 1);
  ue->label = 3;
  tmp_oedge_list.insert(tmp_oedge_list.end(), *oe);
  delete oe;

  ue = AddUEdge(UNDEFINED, v3, v0, -br);
  oe = new OEdge(ue, 1);
  ue->label = 4;
  tmp_oedge_list.insert(tmp_oedge_list.end(), *oe);
  delete oe;

  AddFace(UNDEFINED, tmp_oedge_list);
}

#define MAX_LINE 1000

void TwoComplex::ReadComplex(string filename) {
  FILE* fp;
  char buf[MAX_LINE];
  int c;
  int line_number = 1;

  // hack for now
  NumberField<bigrat>::F = InitCyclotomic<bigrat>(4);

  fp = fopen(filename.c_str(), "r");
  if (fp == nullptr) {
    sprintf(buf, "Error opening file %s", filename.c_str());
    perror(buf);
    ERR_RET("aborting");
  }
  while ((c = fgetc(fp)) != EOF) {
    ungetc(c, fp);

    switch (static_cast<char>(c)) {
      case '#':
        if (fgets(buf, MAX_LINE, fp) == nullptr) {
          ERR_RET("Unexpected EOF");
        }
        line_number++;

        break;

      case 'E':
        read_edge_line(fp, line_number);
        line_number++;
        break;

      case 'F':
        read_face_line(fp, line_number);
        line_number++;
        break;

      case '\n':
        if (fgets(buf, MAX_LINE, fp) == nullptr) {
          ERR_RET("Unexpected EOF");
        }
        break;

      default:
        fprintf(out_f, "bad line: %d\n", line_number);
        if (fgets(buf, MAX_LINE, fp)) {
          ERR_RET("Unexpected EOF");
        }
        ERR_RET(buf);
    }
  }
}

UEdge* TwoComplex::GetUEdge(int id) {
  for (auto i = uedges.begin(); i != uedges.end(); ++i) {
    if ((*i)->id() == id) {
      return (*i);
    }
  }
  return nullptr;
}

Vertex* TwoComplex::GetVertex(int id) {
  for (auto i = vertices.begin(); i != vertices.end(); ++i) {
    if ((*i)->id() == id) {
      return (*i);
    }
  }
  return nullptr;
}

Face* TwoComplex::GetFace(int id) {
  for (auto i = faces.begin(); i != faces.end(); ++i) {
    if ((*i)->id() == id) {
      return (*i);
    }
  }
  return nullptr;
}

char* read_coords(char* s, COORD* p_vec_re, COORD* p_vec_im, int line_number) {
  char* q = nullptr;
  char re[MAX_LINE];
  char im[MAX_LINE];

  //  printf("read coords, s = %s\n", s);

  for (char* p = s; *p != '\0'; p++) {
    if (*p == ')') {
      q = p;
    }
    if (*p == '(' || *p == ',' || *p == ')') {
      *p = ' ';
    }
  }
  if (q == nullptr) {
    ERR_RET2("bad coord: can't find )", line_number);
  }

  if (sscanf(s, "%999s %999s", re, im) != 2) {
    ERR_RET2("bad coord: cant find real and imaginary parts", line_number);
  }
  //  char *junk[0];

  //  printf("s=%s re=%s im=%s\n", s, re, im);

#ifdef USE_LONG_DOUBLE
  *p_vec_re = strtold(re, (char**)0);
  *p_vec_im = strtold(im, (char**)0);
#else
  *p_vec_re = strtod(re, nullptr);
  *p_vec_im = strtod(im, nullptr);
#endif

  //  printf("s=%s re=%s im=%s\n", s, re, im);

  //  printf("p_vec_re = %g, p_vec_im = %g\n", *p_vec_re, *p_vec_im);

  return (q);

  /*
#ifdef USE_LONG_DOUBLE
    if( fscanf(fp,"E%d ( V%d V%d F%d F%d ) (%Lg,%Lg)", &ue_id, &v0_id, &v1_id,
&f0_id, &f1_id, &vec_re, &vec_im) != 7 ) { #else if( fscanf(fp,"E%d ( V%d V%d
F%d F%d ) (%lg,%lg)", &ue_id, &v0_id, &v1_id, &f0_id, &f1_id, &vec_re, &vec_im)
!= 7 ) { #endif ERR_RET2("bad edge line; line =", line_number);
    }

  */
}

void TwoComplex::read_edge_line(FILE* fp, int line_number) {
  int ue_id, v0_id, v1_id, f0_id, f1_id;
  COORD vec_re, vec_im;
  Point vec;

  Vertex *v0, *v1;
  UEdge* ue;
  Face *f0, *f1;
  list<OEdge> tmp_oedge_list;
  char buf[MAX_LINE];

  char* s;

  if (fgets(buf, MAX_LINE, fp) == nullptr) {
    ERR_RET("Unexpected EOF");
  }
  s = strchr(buf, ')');
  if (s == nullptr) {
    ERR_RET2("bad edge line: no )", line_number);
  }
  s++;
  if (*s != ' ') {
    ERR_RET2("bad edge line: no space after )", line_number);
  }
  *s = '\0';
  s++;
  if (sscanf(buf, "E%d ( V%d V%d F%d F%d )", &ue_id, &v0_id, &v1_id, &f0_id,
             &f1_id) != 5) {
    ERR_RET2("bad edge line: cant read edge data", line_number);
  }

  /* skip white space */
  while (*s == ' ') {
    s++;
  }

  if (*s == '(') {
    read_coords(s, &vec_re, &vec_im, line_number);
  } else {
    ERR_RET2("bad edge line: cant find coords", line_number);
  }

  v0 = GetVertex(v0_id);
  if (v0 == nullptr) {
    v0 = AddVertex(v0_id);
  }

  v1 = GetVertex(v1_id);
  if (v1 == nullptr) {
    v1 = AddVertex(v1_id);
  }

  if (f0_id < 0)
    f0 = nullptr;
  else {
    f0 = GetFace(f0_id);
    if (f0 == nullptr) {
      f0 = AddFace(f0_id, tmp_oedge_list);
    }
  }

  if (f1_id < 0)
    f1 = nullptr;
  else {
    f1 = GetFace(f1_id);
    if (f1 == nullptr) {
      f1 = AddFace(f1_id, tmp_oedge_list);
    }
  }
  vec = Point(vec_re, vec_im);

  ue = GetUEdge(ue_id);
  if (ue == nullptr) {
    ue = AddUEdge(ue_id, v0, v1, vec);
  }
  ue->f0 = f0;
  ue->f1 = f1;
  ue->v0 = v0;
  ue->v1 = v1;
  ue->ue_vecQ.cx = Point(vec_re, vec_im);
}

void TwoComplex::read_face_line(FILE* fp, int line_number) {
  char buf[MAX_LINE];
  char* str;
  char c, c1;
  int f_id, e_id, dir = 0;
  Face* f;
  list<OEdge> tmp_oedge_list;
  /* make sure that is null */

  if (fscanf(fp, "%c%d ", &c, &f_id) != 2) {
    ERR_RET2("read_face_line: illegal format; line =", line_number);
  }
  if (c != 'F') {
    ERR_RET2("read_face_line: illegal format; line =", line_number);
  }
  if (fgets(buf, MAX_LINE, fp) == nullptr) {
    ERR_RET("Unexpected EOF");
  }

  str = strtok(buf, " \n");
  while (str != nullptr) {
    if (sscanf(str, "%c%c%d", &c, &c1, &e_id) != 3) {
      ERR_RET2("read_face_line: bad edge; line =", line_number);
    }
    if (c1 != 'E') {
      ERR_RET2("read_face_line: bad edge format; line =", line_number);
    }
    switch (c) {
      case '+':
        dir = 1;
        break;
      case '-':
        dir = -1;
        break;
      default:
        ERR_RET2("read_face_line: bad sign; line =", line_number);
    }
    UEdge* ue;
    ue = GetUEdge(e_id);
    if (ue == nullptr) {
      ue = AddUEdge(e_id, nullptr, nullptr, Point(0, 0));
    }

    OEdge* oe;
    oe = new OEdge(ue, dir);
    tmp_oedge_list.insert(tmp_oedge_list.end(), *oe);

    str = strtok(nullptr, " \n");
  }

  f = GetFace(f_id);

  if (f == nullptr) {
    f = AddFace(f_id, tmp_oedge_list);
  } else {
    f->SetOEdgeList(tmp_oedge_list);
  }
}
