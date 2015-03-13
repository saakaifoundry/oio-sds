/*
OpenIO SDS meta2v2
Copyright (C) 2014 Worldine, original work as part of Redcurrant
Copyright (C) 2015 OpenIO, modified as part of OpenIO Software Defined Storage

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OIO_SDS__meta2v2__meta2_gridd_dispatcher_h
# define OIO_SDS__meta2v2__meta2_gridd_dispatcher_h 1

struct gridd_request_descr_s;

/**
 * The easiest way to retrieve the set of exported function out of
 * the META2 backend.
 *
 * All these functions take a meta1_backend_s as first argument.
 */
const struct gridd_request_descr_s* meta2_gridd_get_v1_requests(void);

/**
 * The easiest way to retrieve the set of exported function out of
 * the META2 backend.
 *
 * All these functions take a meta1_backend_s as first argument.
 */
const struct gridd_request_descr_s* meta2_gridd_get_v2_requests(void);

#endif /*OIO_SDS__meta2v2__meta2_gridd_dispatcher_h*/