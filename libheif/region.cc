/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "region.h"
#include "error.h"


Error RegionItem::parse(const std::vector<uint8_t> &data)
{
  if (data.size() < 8)
  {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Less than 8 bytes of data");
  }

  uint8_t version = data[0];
  (void)version; // version is unused

  uint8_t flags = data[1];
  int field_size = ((flags & 1) ? 32 : 16);

  unsigned int dataOffset;
  if (field_size == 32)
  {
    if (data.size() < 12)
    {
      return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                   "Region data incomplete");
    }
    reference_width =
        ((data[2] << 24) | (data[3] << 16) | (data[4] << 8) | (data[5]));

    reference_height =
        ((data[6] << 24) | (data[7] << 16) | (data[8] << 8) | (data[9]));
    dataOffset = 10;
  }
  else
  {
    reference_width = ((data[2] << 8) | (data[3]));
    reference_height = ((data[4] << 8) | (data[5]));
    dataOffset = 6;
  }

  uint8_t region_count = data[dataOffset];
  dataOffset += 1;
  for (int i = 0; i < region_count; i++)
  {
    uint8_t geometry_type = data[dataOffset];
    dataOffset += 1;

    std::shared_ptr<RegionGeometry> region;

    if (geometry_type == 0)
    {
      region = std::make_shared<RegionGeometry_Point>();
    }
    else if (geometry_type == 1)
    {
      region = std::make_shared<RegionGeometry_Rectangle>();
    }
    else if (geometry_type == 2)
    {
      region = std::make_shared<RegionGeometry_Ellipse>();
    }
    else if (geometry_type == 3)
    {
      auto polygon = std::make_shared<RegionGeometry_Polygon>();
      polygon->closed = true;
      region = polygon;
    }
    else if (geometry_type == 6)
    {
      auto polygon = std::make_shared<RegionGeometry_Polygon>();
      polygon->closed = false;
      region = polygon;
    }
    else
    {
      //     // TODO: this isn't going to work - we can only exit here.
      //   std::cout << "ignoring unsupported region geometry type: "
      //             << (int)geometry_type << std::endl;

      continue;
    }

    Error error = region->parse(data, field_size, &dataOffset);
    if (error)
    {
      return error;
    }

    mRegions.push_back(region);
  }
  return Error::Ok;
}

uint32_t RegionGeometry::parse_unsigned(const std::vector<uint8_t> &data,
                                        int field_size,
                                        unsigned int *dataOffset)
{
  uint32_t x;
  if (field_size == 32)
  {
    x = ((data[*dataOffset] << 24) | (data[*dataOffset + 1] << 16) |
         (data[*dataOffset + 2] << 8) | (data[*dataOffset + 3]));
    *dataOffset = *dataOffset + 4;
  }
  else
  {
    x = ((data[*dataOffset] << 8) | (data[*dataOffset + 1]));
    *dataOffset = *dataOffset + 2;
  }
  return x;
}

int32_t RegionGeometry::parse_signed(const std::vector<uint8_t> &data,
                                     int field_size,
                                     unsigned int *dataOffset)
{
  // TODO: fix this for negative values
  int32_t x;
  if (field_size == 32)
  {
    x = ((data[*dataOffset] << 24) | (data[*dataOffset + 1] << 16) |
         (data[*dataOffset + 2] << 8) | (data[*dataOffset + 3]));
    *dataOffset = *dataOffset + 4;
  }
  else
  {
    x = ((data[*dataOffset] << 8) | (data[*dataOffset + 1]));
    *dataOffset = *dataOffset + 2;
  }
  return x;
}

Error RegionGeometry_Point::parse(const std::vector<uint8_t> &data,
                                  int field_size,
                                  unsigned int *dataOffset)
{
    unsigned int bytesRequired = (field_size / 8) * 2;
    if (data.size() - *dataOffset < bytesRequired)
    {
        return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                    "Insufficient data remaining for point region");
    }
    x = parse_signed(data, field_size, dataOffset);
    y = parse_signed(data, field_size, dataOffset);

  return Error::Ok;
}

Error RegionGeometry_Rectangle::parse(const std::vector<uint8_t> &data,
                                      int field_size,
                                      unsigned int *dataOffset)
{
    unsigned int bytesRequired = (field_size / 8) * 4;
    if (data.size() - *dataOffset < bytesRequired)
    {
        return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                    "Insufficient data remaining for rectangle region");
    }
    x = parse_signed(data, field_size, dataOffset);
    y = parse_signed(data, field_size, dataOffset);
    width = parse_unsigned(data, field_size, dataOffset);
    height = parse_unsigned(data, field_size, dataOffset);
  return Error::Ok;
}


Error RegionGeometry_Ellipse::parse(const std::vector<uint8_t>& data,
                                    int field_size,
                                    unsigned int* dataOffset)
{
  unsigned int bytesRequired = (field_size / 8) * 4;
  if (data.size() - *dataOffset < bytesRequired) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for ellipse region");
  }
  x = parse_signed(data, field_size, dataOffset);
  y = parse_signed(data, field_size, dataOffset);
  radius_x = parse_unsigned(data, field_size, dataOffset);
  radius_y = parse_unsigned(data, field_size, dataOffset);
  return Error::Ok;
}


Error RegionGeometry_Polygon::parse(const std::vector<uint8_t>& data,
                                    int field_size,
                                    unsigned int* dataOffset)
{
  unsigned int bytesRequired1 = (field_size / 8) * 1;
  if (data.size() - *dataOffset < bytesRequired1) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for polygon");
  }

  uint32_t numPoints = parse_unsigned(data, field_size, dataOffset);
  unsigned int bytesRequired2 = (field_size / 8) * numPoints * 2;
  if (data.size() - *dataOffset < bytesRequired2) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for polygon");
  }

  for (uint32_t i = 0; i < numPoints; i++) {
    Point p;
    p.x = parse_signed(data, field_size, dataOffset);
    p.y = parse_signed(data, field_size, dataOffset);
    points.push_back(p);
  }

  return Error::Ok;
}
