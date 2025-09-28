#include "raylib.h"
#include "json.hpp"
#include "earcut.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

#define MAPENGINE_ERR "LakyStrategy::MapEngine::Error: "

using json = nlohmann::json;

using namespace std;

struct Province
{
	string id;

	string name;
	string name_en;
	string name_local;
	Color color;
	int admin_level;

	vector<vector<Vector2>> polygons;
	vector<vector<uint32_t>> polygon_indices;

	// NUTS data
	string country_code;
	float mountain_type;
	float urban_type;
	float coast_type;
	string nuts_level;

};

class MapEngine
{
	private:
		vector<Province> provinces;

		float min_lat, max_lat;
		float min_lon, max_lon;

		int screen_width, screen_height;

		Vector2 geo_to_screen(double lat, double lon)
		{
			Vector2 screen;
			screen.x = (lon - min_lon) / (max_lon - min_lon) * screen_width;
			screen.y = screen_height - (lat - min_lat) / (max_lat - min_lat) * screen_height;
			return screen;
		}

	public:
		MapEngine(int screen_w = 1280, int screen_h = 720) : screen_width(screen_w), screen_height(screen_h)
		{
			min_lat = min_lon = 1e9;
			max_lat = max_lon = -1e9;
		}

		~MapEngine() {}

		bool LoadMap(const string& jsonPath)
		{

			cout << "Loading map definition from " << jsonPath << "..." << endl;

			// Load JSON
			ifstream file(jsonPath);
			if(!file.is_open())
			{
				cerr << MAPENGINE_ERR << "Failed to open map definition JSON with filename " << jsonPath << endl;
				return false;
			}

			json geo_data;
			file >> geo_data;

			cout << "Map definition loaded!" << endl;

			try
			{
				// Calculate bounds
				for(const auto& feature : geo_data.value("features", json::array()))
				{

					auto properties = feature["properties"];
					if(properties.is_null()) continue;
					
					string nuts_level = properties.value("nuts_level", "");
					if(nuts_level != "3" && nuts_level != "0") 
					{
						continue;
					}

					// Print feature being processed
					cout << "Calculating bounds for province " << feature["properties"].value("region_id", "") << endl;

					auto geometry = feature.value("geometry", json{});
					auto coordinates = geometry.value("coordinates", json{});

					string geom_type = geometry.value("type", "");

					if(geometry.is_null() || coordinates.is_null() || geom_type == "")
					{
						throw runtime_error("Invalid geometry data in JSON.");
					}

					if(geom_type == "Polygon")
					{
						for(const auto& ring : coordinates)
						{
							for(const auto& coord : ring)
							{
								double lon = coord[0];
								double lat = coord[1];

								min_lon = min(min_lon, (float)lon);
								max_lon = max(max_lon, (float)lon);

								min_lat = min(min_lat, (float)lat);
								max_lat = max(max_lat, (float)lat);
							}
						}
					}

					else if(geom_type == "MultiPolygon")
					{
						for(const auto& polygon : coordinates)
						{
							for(const auto& ring : polygon)
							{
								for(const auto& coord : ring)
								{
									double lon = coord[0];
									double lat = coord[1];

									min_lon = min(min_lon, (float)lon);
									max_lon = max(max_lon, (float)lon);

									min_lat = min(min_lat, (float)lat);
									max_lat = max(max_lat, (float)lat);
								}
							}
						}
					}
				}

				// Parse features and convert coordinates
				for(const auto& feature : geo_data["features"])
				{

					cout << "Parsing features for province " << feature["properties"].value("region_id", "") << endl;

					Province province;

					auto properties = feature["properties"];

					if(properties.is_null())
					{
						throw runtime_error("Invalid properties data in JSON.");
					}

					//cout << "Doing NUTS level check..." << endl;

					// NUTS level (check if exists first aka not null)
					province.admin_level = properties.value("admin_level", 0);
					province.nuts_level = properties.value("nuts_level", "");


					if(province.admin_level < 4)
					{
						if((province.nuts_level == "3" && province.nuts_level == "0")) {}
						else
						{
							continue; // Skip non-NUTS 3 regions
						}
					}

					//cout << "NUTS level check passed, loading province ID..." << endl;

					province.id = properties.value("region_id", "");

					//cout << "Checking province name..." << endl;

					province.name = properties.value("region_name", "");
					province.name_en = properties.value("region_name_en", "");
					province.name_local = properties.value("region_name_local", "");

					//cout << "Loading additional properties..." << endl;

					province.country_code = properties.value("country_code", "");
					province.mountain_type = properties.value("mount_type", 0.0f);
					province.urban_type = properties.value("urban_type", 0.0f);
					province.coast_type = properties.value("coast_type", 0.0f);

					//cout << "Generating random color..." << endl;

					// Generate random color
					//province.color = Color{ (unsigned char)(rand() % 156 + 100), (unsigned char)(rand() % 156 + 100), (unsigned char)(rand() % 156 + 100), 255 };

					int hash = 0;
					for (char c : province.id) hash += c;

					// Pastel colors
					province.color = {
						(unsigned char)(200 + (hash % 55)),
						(unsigned char)(200 + ((hash * 17) % 55)),
						(unsigned char)(200 + ((hash * 31) % 55)),
						200
					};

					//cout << "Loading geometry..." << endl;

					auto geometry = feature["geometry"];
					auto coordinates = geometry["coordinates"];
					auto geom_type = geometry.value("type", "");

					if(geometry.is_null() || coordinates.is_null())
					{
						throw runtime_error("Invalid geometry data in JSON.");
					}


					if(geom_type == "Polygon")
					{
						// Single polygon
						for(const auto& ring : coordinates)
						{
							vector<Vector2> screen_points;
							for(const auto& coord : ring)
							{
									double lon = coord[0];
									double lat = coord[1];

									screen_points.push_back(geo_to_screen(lat, lon));
							}

							// Triangulate polygons (so that we can render concave polygons yippeee)

							// Check if first point is repeated and remove if it is, then save vertices
							if(!screen_points.empty())
							{
								
								if(screen_points.size() > 1)
								{
									Vector2 &first = screen_points.front();
									Vector2 &last = screen_points.back();

									// If they are the same, remove the last one
									if(fabs(first.x - last.x) < 1e-6f && fabs(first.y - last.y) < 1e-6f)
									{
										screen_points.pop_back();
									}
								}

								province.polygons.push_back(screen_points);
							}

							// Triangulation
							if(!screen_points.empty())
							{
								vector<vector<array<double, 2>>> rings;

								rings.emplace_back();
								rings[0].reserve(screen_points.size());

								for(const auto &p : screen_points)
								{
									rings[0].push_back({ (double)p.x, (double)p.y });
								}

								vector<uint32_t> indices = mapbox::earcut<uint32_t>(rings);
								province.polygon_indices.push_back(move(indices));
							}

						}
					}

					else if(geom_type == "MultiPolygon")
					{
						// Multiple polygons
						for(const auto& polygon : coordinates)
						{
							for(const auto& ring : polygon)
							{
								vector<Vector2> screen_points;
								for(const auto& coord : ring)
								{
										double lon = coord[0];
										double lat = coord[1];

										screen_points.push_back(geo_to_screen(lat, lon));
								}

								// Triangulate polygons (so that we can render concave polygons yippeee)

								// Check if first point is repeated and remove if it is, then save vertices
								if(!screen_points.empty())
								{
									
									if(screen_points.size() > 1)
									{
										Vector2 &first = screen_points.front();
										Vector2 &last = screen_points.back();

										// If they are the same, remove the last one
										if(fabs(first.x - last.x) < 1e-6f && fabs(first.y - last.y) < 1e-6f)
										{
											screen_points.pop_back();
										}
									}

									province.polygons.push_back(screen_points);
								}

								// Triangulation
								if(!screen_points.empty())
								{
									vector<vector<array<double, 2>>> rings;

									rings.emplace_back();
									rings[0].reserve(screen_points.size());

									for(const auto &p : screen_points)
									{
										rings[0].push_back({ (double)p.x, (double)p.y });
									}

									vector<uint32_t> indices = mapbox::earcut<uint32_t>(rings);
									province.polygon_indices.push_back(move(indices));
								}
							}
						}
					}

					if(!province.polygons.empty())
					{
						provinces.push_back(province);
					}

					cout << "Loaded province " << province.id << " with " << province.polygons.size() << " polygons." << endl;

				}



				cout << "Sucessfully loaded " << provinces.size() << " provinces!" << endl;

				return true;
				

				
			}
			catch(const exception& e)
			{
				cerr << MAPENGINE_ERR << e.what() << endl;
				return false;
			}
			

		}

		const vector<Province>& getProvinces() const { return provinces; }

		void render()
		{

			DrawRectangle(100, 100, 200, 100, RED);
			DrawText("Test render", 110, 130, 20, WHITE);

			DrawTriangle( (Vector2){100,100}, (Vector2){150,200}, (Vector2){200,100}, GREEN );

			for(const auto& province : provinces)
			{
				const Color fill_color = province.color;
				const Color edge_color = DARKGRAY;

				// Loop thru each stored polygon
				for(size_t poly_index = 0; poly_index < province.polygons.size(); ++poly_index)
				{
					const auto& poly = province.polygons[poly_index];

					// Check if on screen
					Rectangle poly_bounds = { poly[0].x, poly[0].y, 0, 0 };
					for(const auto& p : poly)
					{
						poly_bounds.x = min(poly_bounds.x, p.x);
						poly_bounds.y = min(poly_bounds.y, p.y);
						poly_bounds.width = max(poly_bounds.width, p.x);
						poly_bounds.height = max(poly_bounds.height, p.y);
					}
					poly_bounds.width -= poly_bounds.x;
					poly_bounds.height -= poly_bounds.y;
					Rectangle screen_rect = { 0, 0, (float)screen_width, (float)screen_height };
					if(!CheckCollisionRecs(poly_bounds, screen_rect))
					{
						continue; // Skip drawing this polygon
					}

					if(poly_index < province.polygon_indices.size())
					{
						const auto &indices = province.polygon_indices[poly_index];

						// Draw triangles
						for(size_t i = 0; i + 2 < indices.size(); i += 3)
						{
							// Get index of vertices from the indices array
							uint32_t indexA = indices[i+0];
							uint32_t indexB = indices[i+1];
							uint32_t indexC = indices[i+2];

							// Skip freaky beaky indices
							if(indexA >= poly.size() || indexB >= poly.size() || indexC >= poly.size()) continue;

							DrawTriangle(poly[indexA], poly[indexC], poly[indexB], fill_color);
							//DrawTriangleLines(poly[indexA], poly[indexB], poly[indexC], edge_color);
						}
					}
					else
					{
						// If we do not have cached indices somehow??? then draw an outline ig
						for(size_t k = 0; k < poly.size(); ++k)
						{
							DrawLineV(poly[k], poly[(k+1) % poly.size()], edge_color);
						}
					}
				}
			}
		}

		Province getProvinceAt(int x, int y)
		{
			Vector2 point = {(float)x, (float)y};

			for(const auto& province : provinces)
			{
				for(const auto& polygon : province.polygons)
				{
					// Simplified point-in-polygon check
					bool inside = false;
					for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
						if (((polygon[i].y > point.y) != (polygon[j].y > point.y)) &&
							(point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) / 
							(polygon[j].y - polygon[i].y) + polygon[i].x)) {
							inside = !inside;
						}
					}
					if (inside) {
						return province;
					}
				}
			}

			return Province{};
		}

		void setProvinceColor(const string& id, const Color& color)
		{
			for(auto& province : provinces)
			{
				if(province.id == id)
				{
					province.color = color;
					return;
				}
			}
		}

		Province getProvinceByID(const string& id)
		{
			for(const auto& province : provinces)
			{
				if(province.id == id)
				{
					return province;
				}
			}

			return Province{};
		}
};