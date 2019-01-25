/**********************************************************************
 *  This file is part of Polygon.
 *
 *        Copyright (C) 2018 Alex Eskin
 *        Copyright (C) 2019 Julian Rüth
 *
 *  Polygon is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Polygon is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Polygon. If not, see <https://www.gnu.org/licenses/>.
 *********************************************************************/

#ifndef LIBPOLYGON_VECTOR_H
#define LIBPOLYGON_VECTOR_H

#include <vector>

namespace polygon {
template <class T>
std::vector<T> &operator+=(std::vector<T> &v, std::vector<T> w);

template <class T>
std::vector<T> operator+(std::vector<T> v, std::vector<T> w);

template <class T>
std::vector<T> operator*(T v, std::vector<T> w);
}  // namespace polygon
#endif  // LIBPOLYGON_VECTOR_H
