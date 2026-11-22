#include "Body.h"

#include "Settings.h"

// The skin-wrap code (vertex decoding, skinning, the region test) is adapted from
// OStrands' Body.cpp by LeonVanWinkle (Saint), GPL-3.0, by way of Lacrima's Face.cpp.
// Lacrima reads one face; this reads every skinned mesh on the torso (the body, or
// what is worn over it), finds the nipples on them, and then poses each frame only
// the surface near the fluid and around the nipples.

namespace Lactis::Body
{
	namespace
	{
		constexpr float         kWrapVoxel = 0.25f;
		constexpr float         kForget = 10.0f;
		constexpr float         kRecheck = 2.0f;
		constexpr std::uint32_t kVertexBudget = 200000;   // one shape
		constexpr std::size_t   kPointBudget = 250000;    // one actor
		constexpr std::size_t   kNone = static_cast<std::size_t>(-1);
		// Skin within this of a nipple (bind units) is posed every frame, so a welling
		// drop always has surface under it, whatever the region says.
		constexpr float kDuctReach = 2.5f;
		constexpr float kWatchSlack = 0.5f;
		constexpr float kRegionSlack = 2.0f;
		// A joint whose skin moved this far against a side's bone since last frame
		// (bind units, 0.4 mm) is moving the surface under that side's fluid.
		constexpr float kMoveStep = 0.03f;
		// The nipple: of the vertices weighted to the breast's tip bone, the ones within
		// this share of the heaviest weight, and of those the ones this close (units) to
		// the furthest out along their mean normal.
		constexpr float       kTipWeight = 0.85f;
		constexpr float       kTipWeightLoose = 0.6f;
		constexpr float       kTipWeightFloor = 0.2f;
		constexpr std::size_t kTipVertices = 6;
		constexpr float       kTipDepth = 0.15f;
		// A shape must carry this share of the breast vertices the widest shape on the same tip bone carries
		// to be taken as the surface over the nipple.
		constexpr float       kCoverShare = 0.25f;
		// A garment covers a nipple when the ray out of the body's nipple meets it this
		// far out (or this far inside the skin, where it clips), and at least this much
		// of it (square units) lies within this radius of the nipple: a ring or a
		// pasty does not cover it, a cup or a cuirass does.
		constexpr float kCoverReach = 8.0f;
		constexpr float kCoverBehind = 1.0f;
		constexpr float kCoverRadius = 4.0f;
		constexpr float kCoverArea = 16.0f;
		// With no mesh weighted to a breast bone, the nipple is the surface nearest that
		// bone, when it is this close (units).
		constexpr float kBoneReach = 8.0f;

		struct Joint
		{
			RE::NiPointer<RE::NiAVObject> bone;
			RE::NiTransform               skinToBone;
		};

		struct Point
		{
			RE::NiPoint3                 position;
			RE::NiPoint3                 normal;
			std::array<std::uint16_t, 4> joints{};
			std::array<float, 4>         weights{};
			std::uint8_t                 dominant{ 0 };
		};

		struct Mesh
		{
			std::vector<RE::NiPoint3>                positions;
			std::vector<RE::NiPoint3>                normals;
			std::vector<std::array<float, 4>>        weights;
			std::vector<std::array<std::uint8_t, 4>> bones;
			std::vector<std::array<std::uint32_t, 3>> triangles;
		};

		struct Box
		{
			RE::NiPoint3 low;
			RE::NiPoint3 high;
		};

		// One shape's answer to "where is this side's nipple".
		struct Tip
		{
			std::size_t                   begin{ 0 };  // the shape's points
			std::size_t                   end{ 0 };
			RE::NiPoint3                  position;    // skin space
			RE::NiPoint3                  normal;
			RE::NiPoint3                  inBone;      // the same in the tip bone's space, where shapes compare
			RE::NiPoint3                  axisInBone;
			RE::NiPointer<RE::NiAVObject> bone;
			std::string                   boneName;
			std::string                   shape;
			std::size_t                   vertices{ 0 };
			std::size_t                   coverage{ 0 };  // vertices the tip bone carries at all: how much of the breast the shape covers
			RE::BSGeometry*               geometry{ nullptr };  // valid during Build only
			bool                          skin{ false };        // a skin-shaded shape (the body), not clothing
		};

		// A worn shape that is not skin, kept through one Build to test what lies
		// over the nipples.
		struct Cover
		{
			RE::BSGeometry*                           geometry{ nullptr };
			std::string                               name;
			std::vector<RE::NiPoint3>                 positions;
			std::vector<std::array<std::uint32_t, 3>> triangles;
		};

		using Signature = std::vector<std::pair<const void*, const void*>>;

		struct Torso
		{
			RE::NiPointer<RE::NiAVObject>                        root;
			Signature                                            signature;
			std::vector<Joint>                                   joints;
			std::vector<Point>                                   points;
			std::vector<Patch>                                   patches;
			std::vector<std::uint32_t>                           candidates;
			std::vector<std::uint32_t>                           aroundDucts;
			std::vector<std::uint64_t>                           stamp;
			std::vector<Box>                                     boxes;
			std::vector<Box>                                     watchBoxes;
			std::vector<RE::NiTransform>                         world;
			std::vector<std::uint8_t>                            moved;
			std::array<std::size_t, kEyes>                       nipple{ kNone, kNone };
			std::array<RE::NiPointer<RE::NiAVObject>, kEyes>     bone;
			std::array<std::vector<std::array<RE::NiPoint3, 2>>, kEyes> relative;  // each joint's two probes in the side's bone space, last frame
			std::array<bool, kEyes>                              moving{};
			std::array<std::string, kEyes>                       how;
			std::array<std::string, kEyes>                       cover;  // what covers each nipple; empty = bare
			Ducts                                                ducts{};
			float                                                scale{ 1.0f };
			std::string                                          report;
			std::uint64_t                                        frame{ 0 };
			bool                                                 regionPosed{ false };
			Region                                               region{};
			float                                                seen{ 0.0f };
			float                                                checked{ 0.0f };
			bool                                                 ok{ false };
		};

		std::unordered_map<std::uint32_t, Torso> torsos;
		std::uint64_t                            frame{ 1 };
		float                                    clock{ 0.0f };

		float Half(std::uint16_t a_value)
		{
			const std::uint32_t sign = static_cast<std::uint32_t>(a_value & 0x8000u) << 16;
			std::uint32_t       exponent = (a_value >> 10) & 0x1Fu;
			std::uint32_t       mantissa = a_value & 0x3FFu;
			std::uint32_t       bits = sign;
			if (exponent == 0) {
				if (mantissa != 0) {
					exponent = 113;
					while (!(mantissa & 0x400u)) {
						mantissa <<= 1;
						--exponent;
					}
					bits |= (exponent << 23) | ((mantissa & 0x3FFu) << 13);
				}
			} else if (exponent == 31) {
				bits |= 0x7F800000u | (mantissa << 13);
			} else {
				bits |= ((exponent + 112) << 23) | (mantissa << 13);
			}
			return std::bit_cast<float>(bits);
		}

		std::uint32_t StrideOf(const RE::BSGraphics::VertexDesc& a_desc)
		{
			return static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(a_desc) & 0xF) * 4;
		}

		bool FullPrecision(const RE::BSGraphics::VertexDesc& a_desc)
		{
			std::uint32_t position = StrideOf(a_desc);
			for (std::uint8_t attribute = RE::BSGraphics::Vertex::VA_TEXCOORD0; attribute < RE::BSGraphics::Vertex::VA_COUNT; ++attribute) {
				const auto flag = static_cast<RE::BSGraphics::Vertex::Flags>(1u << attribute);
				if (a_desc.HasFlag(flag)) {
					position = std::min(position, a_desc.GetAttributeOffset(static_cast<RE::BSGraphics::Vertex::Attribute>(attribute)));
				}
			}
			return position >= 12;
		}

		RE::NiPoint3 PositionOf(const std::uint8_t* a_vertex, bool a_full)
		{
			if (a_full) {
				std::array<float, 3> xyz{};
				std::memcpy(xyz.data(), a_vertex, sizeof(xyz));
				return { xyz[0], xyz[1], xyz[2] };
			}
			std::array<std::uint16_t, 3> xyz{};
			std::memcpy(xyz.data(), a_vertex, sizeof(xyz));
			return { Half(xyz[0]), Half(xyz[1]), Half(xyz[2]) };
		}

		RE::NiPoint3 Unit(RE::NiPoint3 a_vector, const RE::NiPoint3& a_fallback)
		{
			return a_vector.Unitize() > 1e-5f ? a_vector : a_fallback;
		}

		RE::NiSkinPartition* PartitionOf(RE::BSGeometry* a_geometry)
		{
			auto* skin = a_geometry ? a_geometry->GetGeometryRuntimeData().skinInstance.get() : nullptr;
			return skin ? skin->skinPartition.get() : nullptr;
		}

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		std::string NameOf(const RE::NiAVObject* a_object)
		{
			const char* name = a_object ? a_object->name.c_str() : nullptr;
			return name && *name ? name : "?";
		}

		// Not drawn, part of the head the game builds (hair, face, brows: none of it
		// torso), or one of Lactis's own milk meshes.
		bool Skipped(RE::NiAVObject* a_object, RE::NiAVObject* a_root)
		{
			for (auto* node = a_object; node && node != a_root; node = node->parent) {
				if (node->GetAppCulled()) {
					return true;
				}
				const char* name = node->name.c_str();
				if (name && (std::string_view(name) == kMilkRoot || std::string_view(name).starts_with("BSFaceGenNiNode"))) {
					return true;
				}
			}
			return false;
		}

		// A bone of one breast: "L Breast03", "NPC R Breast", not "NPC L PreBreast".
		// a_rank orders a chain of them from the chest out (the digits at the end).
		//
		// A soft body (GT Softbody) has no such chain: each breast is a net of bones
		// "Soft_L<spoke><ring>" (ring 00 at the chest, 03 outermost) closed by one bone
		// on the nipple, "Soft_LN00". Those count as breast bones, the nipple's ranked
		// above any chain bone a mesh may also carry, since physics moves only the net.
		bool BreastBone(std::string_view a_name, std::size_t& a_side, int& a_rank)
		{
			const std::string name = Lower(a_name);
			if (name.size() == 9 && name.starts_with("soft_") && (name[5] == 'l' || name[5] == 'r')) {
				a_side = name[5] == 'l' ? kLeft : kRight;
				if (name.compare(6, 3, "n00") == 0) {
					a_rank = 1000;
					return true;
				}
				if (std::isdigit(static_cast<unsigned char>(name[6])) && std::isdigit(static_cast<unsigned char>(name[7])) &&
					std::isdigit(static_cast<unsigned char>(name[8]))) {
					a_rank = 100 + (name[7] - '0') * 10 + (name[8] - '0');
					return true;
				}
				return false;
			}
			if (name.find("breast") == std::string::npos || name.find("pre") != std::string::npos) {
				return false;
			}
			const std::string padded = " " + name;
			if (padded.find(" l ") != std::string::npos) {
				a_side = kLeft;
			} else if (padded.find(" r ") != std::string::npos) {
				a_side = kRight;
			} else {
				return false;
			}
			a_rank = 0;
			int place = 1;
			for (auto it = name.rbegin(); it != name.rend() && std::isdigit(static_cast<unsigned char>(*it)); ++it) {
				a_rank += (*it - '0') * place;
				place *= 10;
			}
			return true;
		}

		// Bones whose skin a drop can run over on its way down.
		bool TorsoBone(std::string_view a_name)
		{
			const std::string name = Lower(a_name);
			for (const char* part : { "breast", "soft_", "spine", "belly", "pelvis", "thigh" }) {
				if (name.find(part) != std::string::npos) {
					return true;
				}
			}
			return false;
		}

		bool ReadMesh(RE::BSGeometry* a_geometry, Mesh& a_out)
		{
			auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
			auto* partition = skin ? skin->skinPartition.get() : nullptr;
			if (!skin || !skin->skinData || !skin->bones || !partition || !partition->numPartitions ||
				partition->vertexCount == 0 || partition->vertexCount > kVertexBudget) {
				return false;
			}
			const auto& first = partition->partitions[0];
			if (!first.buffData || !first.buffData->rawVertexData) {
				return false;
			}
			const std::uint32_t stride = StrideOf(first.vertexDesc);
			const std::uint32_t skinning = first.vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
			if (stride < 8 || stride > 128 || !first.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED) ||
				!first.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) || skinning + 12 > stride) {
				return false;
			}
			const std::uint32_t count = partition->vertexCount;
			const bool          full = FullPrecision(first.vertexDesc);
			const std::uint8_t* data = first.buffData->rawVertexData;
			a_out.positions.resize(count);
			a_out.weights.resize(count);
			a_out.bones.resize(count);
			for (std::uint32_t v = 0; v < count; ++v) {
				const std::uint8_t* vertex = data + static_cast<std::size_t>(v) * stride;
				a_out.positions[v] = PositionOf(vertex, full);
				std::array<std::uint16_t, 4> halves{};
				std::memcpy(halves.data(), vertex + skinning, sizeof(halves));
				std::memcpy(a_out.bones[v].data(), vertex + skinning + 8, sizeof(a_out.bones[v]));
				for (std::size_t j = 0; j < halves.size(); ++j) {
					a_out.weights[v][j] = Half(halves[j]);
				}
			}
			a_out.normals.assign(count, RE::NiPoint3{});
			a_out.triangles.clear();
			for (std::uint32_t p = 0; p < partition->numPartitions; ++p) {
				const auto& part = partition->partitions[p];
				if (!part.triList) {
					continue;
				}
				std::uint32_t highest = 0;
				for (std::uint32_t i = 0; i < part.triangles * 3u; ++i) {
					highest = std::max<std::uint32_t>(highest, part.triList[i]);
				}
				const bool local = part.vertexMap && highest < part.vertices && part.vertices < count;
				const auto index = [&](std::uint32_t a_corner) -> std::uint32_t {
					const std::uint32_t value = part.triList[a_corner];
					return local ? part.vertexMap[value] : value;
				};
				for (std::uint32_t t = 0; t < part.triangles; ++t) {
					const std::uint32_t a = index(3 * t);
					const std::uint32_t b = index(3 * t + 1);
					const std::uint32_t c = index(3 * t + 2);
					if (a >= count || b >= count || c >= count) {
						continue;
					}
					a_out.triangles.push_back({ a, b, c });
					const RE::NiPoint3 normal = (a_out.positions[b] - a_out.positions[a]).Cross(a_out.positions[c] - a_out.positions[a]);
					a_out.normals[a] += normal;
					a_out.normals[b] += normal;
					a_out.normals[c] += normal;
				}
			}
			return true;
		}

		// +1 when the mesh's triangle normals point away from its middle, -1 when its
		// winding has them point in.
		float Winding(const Mesh& a_mesh)
		{
			RE::NiPoint3 centroid;
			for (const auto& position : a_mesh.positions) {
				centroid += position;
			}
			centroid = centroid / static_cast<float>(std::max<std::size_t>(a_mesh.positions.size(), 1));
			std::size_t facing = 0;
			std::size_t outward = 0;
			for (std::size_t v = 0; v < a_mesh.positions.size(); ++v) {
				if (a_mesh.normals[v].SqrLength() < 1e-12f) {
					continue;
				}
				++facing;
				outward += a_mesh.normals[v].Dot(a_mesh.positions[v] - centroid) > 0.0f;
			}
			return facing && outward * 2 < facing ? -1.0f : 1.0f;
		}

		// Adds a mesh's torso vertices as wrap points, deduplicated on a fine voxel.
		// Returns how many points it added.
		std::size_t Add(Torso& a_torso, RE::BSGeometry* a_geometry, const Mesh& a_mesh, float a_flip)
		{
			auto* skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
			if (!skin || !skin->skinData || !skin->bones) {
				return 0;
			}
			const std::uint32_t       bones = skin->skinData->GetBoneCount();
			std::vector<std::uint8_t> torso(bones, 0);
			bool                      any = false;
			for (std::uint32_t i = 0; i < bones; ++i) {
				const char* name = skin->bones[i] ? skin->bones[i]->name.c_str() : nullptr;
				torso[i] = name && TorsoBone(name);
				any = any || torso[i];
			}
			if (!any) {
				return 0;
			}
			const std::size_t base = a_torso.joints.size();
			for (std::uint32_t i = 0; i < bones; ++i) {
				a_torso.joints.push_back({ RE::NiPointer<RE::NiAVObject>(skin->bones[i]), skin->skinData->GetBoneDataSkinToBone(i) });
			}
			const auto cell = [](float a_value) {
				return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(a_value / kWrapVoxel)) + (1 << 19)) & 0xFFFFF;
			};
			std::unordered_set<std::uint64_t> taken;
			std::size_t                       kept = 0;
			for (std::uint32_t v = 0; v < a_mesh.positions.size() && a_torso.points.size() < kPointBudget; ++v) {
				RE::NiPoint3 normal = a_mesh.normals[v] * a_flip;
				if (normal.Unitize() < 1e-6f) {
					continue;
				}
				const auto& position = a_mesh.positions[v];
				if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
					continue;
				}
				Point point{ position, normal, {}, {} };
				float total = 0.0f;
				float heaviest = 0.0f;
				bool  onTorso = false;
				for (std::size_t j = 0; j < 4; ++j) {
					const std::uint8_t bone = a_mesh.bones[v][j];
					const float        weight = a_mesh.weights[v][j];
					if (bone < bones && skin->bones[bone] && std::isfinite(weight) && weight > 0.0f) {
						point.joints[j] = static_cast<std::uint16_t>(base + bone);
						point.weights[j] = weight;
						total += weight;
						if (weight > heaviest) {
							heaviest = weight;
							point.dominant = static_cast<std::uint8_t>(j);
							onTorso = torso[bone] != 0;
						}
					}
				}
				if (!(total > 0.0f) || !onTorso) {
					continue;
				}
				const std::uint64_t key = (cell(position.x) << 43) | (cell(position.y) << 23) | (cell(position.z) << 3) |
				                          (normal.x > 0.0f ? 4u : 0u) | (normal.y > 0.0f ? 2u : 0u) | (normal.z > 0.0f ? 1u : 0u);
				if (!taken.insert(key).second) {
					continue;
				}
				for (auto& weight : point.weights) {
					weight /= total;
				}
				a_torso.points.push_back(point);
				++kept;
			}
			if (kept == 0) {
				a_torso.joints.resize(base);
			}
			return kept;
		}

		// Where this shape puts one side's nipple. The weights of a breast's tip bone
		// peak at the nipple on every body that has such a bone, and a vertex keeps its
		// weights under any slider or morph, so the heaviest vertices mark it whatever
		// the breast's size or shape; among them the tip is the one furthest out. A
		// body with one bone per breast has the whole breast at full weight, and the
		// same rule gives its most protruding point. Clothes copied from a body carry
		// its weights, so they answer the same way.
		std::optional<Tip> FindTip(RE::BSGeometry* a_geometry, const Mesh& a_mesh, float a_flip, std::size_t a_side)
		{
			auto*               skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
			const std::uint32_t bones = skin->skinData->GetBoneCount();
			std::uint32_t       tipBone = bones;
			int                 best = -1;
			for (std::uint32_t i = 0; i < bones; ++i) {
				const char* name = skin->bones[i] ? skin->bones[i]->name.c_str() : nullptr;
				std::size_t side = 0;
				int         rank = 0;
				if (name && BreastBone(name, side, rank) && side == a_side && rank > best) {
					best = rank;
					tipBone = i;
				}
			}
			if (tipBone == bones) {
				return std::nullopt;
			}
			const std::size_t  count = a_mesh.positions.size();
			std::vector<float> weight(count, 0.0f);
			float              heaviest = 0.0f;
			for (std::size_t v = 0; v < count; ++v) {
				if (a_mesh.normals[v].SqrLength() < 1e-12f) {
					continue;
				}
				for (std::size_t j = 0; j < 4; ++j) {
					if (a_mesh.bones[v][j] == tipBone && std::isfinite(a_mesh.weights[v][j])) {
						weight[v] += a_mesh.weights[v][j];
					}
				}
				heaviest = std::max(heaviest, weight[v]);
			}
			if (heaviest < kTipWeightFloor) {
				return std::nullopt;
			}
			std::vector<std::uint32_t> top;
			for (const float share : { kTipWeight, kTipWeightLoose }) {
				top.clear();
				for (std::size_t v = 0; v < count; ++v) {
					if (weight[v] >= share * heaviest) {
						top.push_back(static_cast<std::uint32_t>(v));
					}
				}
				if (top.size() >= kTipVertices) {
					break;
				}
			}
			if (top.size() < kTipVertices) {
				return std::nullopt;
			}
			RE::NiPoint3 centre;
			RE::NiPoint3 axis;
			for (const auto v : top) {
				centre += a_mesh.positions[v];
				axis += Unit(a_mesh.normals[v] * a_flip, RE::NiPoint3{});
			}
			centre = centre / static_cast<float>(top.size());
			if (axis.Unitize() < 1e-4f) {
				return std::nullopt;
			}
			float apex = -std::numeric_limits<float>::max();
			for (const auto v : top) {
				apex = std::max(apex, (a_mesh.positions[v] - centre).Dot(axis));
			}
			Tip          tip;
			RE::NiPoint3 normal;
			std::size_t  close = 0;
			for (const auto v : top) {
				if ((a_mesh.positions[v] - centre).Dot(axis) >= apex - kTipDepth) {
					tip.position += a_mesh.positions[v];
					normal += Unit(a_mesh.normals[v] * a_flip, RE::NiPoint3{});
					++close;
				}
			}
			tip.position = tip.position / static_cast<float>(close);
			tip.normal = Unit(normal, axis);
			const auto toBone = skin->skinData->GetBoneDataSkinToBone(tipBone);
			tip.inBone = toBone * tip.position;
			tip.axisInBone = toBone.rotate * axis;
			tip.bone.reset(skin->bones[tipBone]);
			tip.boneName = NameOf(skin->bones[tipBone]);
			tip.shape = NameOf(a_geometry);
			tip.vertices = top.size();
			tip.coverage = static_cast<std::size_t>(std::ranges::count_if(weight, [](float a_weight) { return a_weight >= kTipWeightFloor; }));
			return tip;
		}

		// Poses one wrap point with the joints as they stand this frame.
		void SkinPoint(const Torso& a_torso, const Point& a_point, RE::NiPoint3& a_position, RE::NiPoint3& a_normal, float& a_scale)
		{
			RE::NiPoint3 position;
			RE::NiPoint3 normal;
			float        scale = 0.0f;
			for (std::size_t j = 0; j < 4; ++j) {
				const float weight = a_point.weights[j];
				if (weight <= 0.0f) {
					continue;
				}
				const auto& transform = a_torso.world[a_point.joints[j]];
				position += (transform * a_point.position) * weight;
				normal += (transform.rotate * a_point.normal) * weight;
				scale += transform.scale * weight;
			}
			a_position = position;
			a_normal = Unit(normal, a_point.normal);
			a_scale = scale;
		}

		void WorldOf(Torso& a_torso)
		{
			auto& world = a_torso.world;
			world.assign(a_torso.joints.size(), RE::NiTransform{});
			for (std::size_t j = 0; j < a_torso.joints.size(); ++j) {
				const auto& joint = a_torso.joints[j];
				if (joint.bone) {
					world[j] = joint.bone->world * joint.skinToBone;
				}
			}
		}

		Signature SignatureOf(RE::NiAVObject* a_root)
		{
			Signature signature;
			RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geometry) {
				auto* partition = PartitionOf(a_geometry);
				if (partition && partition->vertexCount > 0 && !Skipped(a_geometry, a_root)) {
					// skee swaps in a new partition whenever it re-applies body morphs, so
					// the pointer changing means the vertices did
					signature.emplace_back(a_geometry, partition);
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return signature;
		}

		// The body's own surface: skin-shaded (the game's skin tint shader), where a
		// garment or armor is not.
		bool SkinShaded(RE::BSGeometry* a_geometry)
		{
			auto* shader = a_geometry->GetGeometryRuntimeData().shaderProperty.get();
			auto* material = shader ? shader->GetBaseMaterial() : nullptr;
			return material && material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint;
		}

		// Where the ray a_origin + t * a_direction meets the triangle, either face.
		bool Hit(const RE::NiPoint3& a_origin, const RE::NiPoint3& a_direction, const RE::NiPoint3& a_a, const RE::NiPoint3& a_b,
			const RE::NiPoint3& a_c, float& a_t)
		{
			const RE::NiPoint3 edge1 = a_b - a_a;
			const RE::NiPoint3 edge2 = a_c - a_a;
			const RE::NiPoint3 p = a_direction.Cross(edge2);
			const float        det = edge1.Dot(p);
			if (std::abs(det) < 1e-9f) {
				return false;
			}
			const float        inverse = 1.0f / det;
			const RE::NiPoint3 s = a_origin - a_a;
			const float        u = s.Dot(p) * inverse;
			if (u < 0.0f || u > 1.0f) {
				return false;
			}
			const RE::NiPoint3 q = s.Cross(edge1);
			const float        v = a_direction.Dot(q) * inverse;
			if (v < 0.0f || u + v > 1.0f) {
				return false;
			}
			a_t = edge2.Dot(q) * inverse;
			return true;
		}

		// What covers one nipple: the name of the garment, or empty when it is bare.
		// From the body's own nipple (the skin-shaded shape weighted to the breast) a
		// ray goes out along the nipple's direction; a garment it meets covers it, if
		// enough of that garment lies around the nipple to be clothing and not a ring
		// or a pasty. The two shapes are compared in the bind space of a bone both are
		// bound to, so the pose and the breast physics do not enter into it.
		std::string CoverOf(const std::vector<Tip>& a_tips, const std::vector<Cover>& a_covers, RE::Actor* a_actor)
		{
			const Tip* bare = nullptr;
			for (const auto& tip : a_tips) {
				if (tip.skin && (!bare || tip.coverage > bare->coverage)) {
					bare = &tip;
				}
			}
			if (!bare) {
				// No skin weighted to this breast. Under a body armor that brings no body
				// of its own (or a rigid cuirass) the breast is covered; with none, the
				// body is just not skin-shaded, and nothing is known to cover it.
				auto* worn = a_actor ? a_actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody) : nullptr;
				if (!worn) {
					return {};
				}
				const char* name = worn->GetName();
				return std::format("{} (no skin under it)", name && *name ? name : "a body armor");
			}
			auto*      body = bare->geometry->GetGeometryRuntimeData().skinInstance.get();
			const auto bodyBones = body->skinData->GetBoneCount();
			for (const auto& cover : a_covers) {
				auto*      skin = cover.geometry->GetGeometryRuntimeData().skinInstance.get();
				const auto bones = skin->skinData->GetBoneCount();
				// a bone both are bound to: the nipple's own if the garment has it, else
				// any torso bone they share
				std::uint32_t mine = bodyBones;
				std::uint32_t theirs = bones;
				for (std::uint32_t pass = 0; pass < 2 && theirs == bones; ++pass) {
					for (std::uint32_t i = 0; i < bodyBones && theirs == bones; ++i) {
						auto* bone = body->bones[i];
						if (!bone || (pass == 0 ? bone != bare->bone.get() : !TorsoBone(NameOf(bone)))) {
							continue;
						}
						for (std::uint32_t j = 0; j < bones; ++j) {
							if (skin->bones[j] == bone) {
								mine = i;
								theirs = j;
								break;
							}
						}
					}
				}
				if (theirs == bones) {
					continue;
				}
				const auto         toMine = body->skinData->GetBoneDataSkinToBone(mine);
				const auto         toTheirs = skin->skinData->GetBoneDataSkinToBone(theirs);
				const RE::NiPoint3 origin = toMine * bare->position;
				const RE::NiPoint3 direction = Unit(toMine.rotate * bare->normal, RE::NiPoint3{});
				if (direction.SqrLength() < 0.5f) {
					continue;
				}
				std::vector<RE::NiPoint3> at(cover.positions.size());
				for (std::size_t v = 0; v < at.size(); ++v) {
					at[v] = toTheirs * cover.positions[v];
				}
				float                      area = 0.0f;
				std::vector<std::uint32_t> around;
				for (std::uint32_t t = 0; t < cover.triangles.size(); ++t) {
					const auto& [a, b, c] = cover.triangles[t];
					const RE::NiPoint3 middle = (at[a] + at[b] + at[c]) / 3.0f;
					if (middle.GetSquaredDistance(origin) > kCoverRadius * kCoverRadius) {
						continue;
					}
					area += 0.5f * (at[b] - at[a]).Cross(at[c] - at[a]).Length();
					around.push_back(t);
				}
				if (area < kCoverArea) {
					continue;
				}
				for (const auto t : around) {
					const auto& [a, b, c] = cover.triangles[t];
					float       distance = 0.0f;
					if (Hit(origin, direction, at[a], at[b], at[c], distance) && distance >= -kCoverBehind && distance <= kCoverReach) {
						return cover.name;
					}
				}
			}
			return {};
		}

		void Build(Torso& a_torso, RE::Actor* a_actor)
		{
			a_torso.joints.clear();
			a_torso.points.clear();
			a_torso.patches.clear();
			a_torso.candidates.clear();
			a_torso.aroundDucts.clear();
			a_torso.stamp.clear();
			a_torso.nipple = { kNone, kNone };
			a_torso.bone = {};
			a_torso.relative = {};
			a_torso.moving = {};
			a_torso.how = {};
			a_torso.cover = {};
			a_torso.ducts = {};
			a_torso.ok = false;
			a_torso.report.clear();
			auto* root = a_torso.root.get();

			std::array<std::vector<Tip>, kEyes> tips;
			std::vector<Cover>                  covers;
			std::size_t                         shapes = 0;
			Mesh                                mesh;
			for (const auto& [object, partition] : a_torso.signature) {
				auto* geometry = const_cast<RE::BSGeometry*>(static_cast<const RE::BSGeometry*>(object));
				if (!ReadMesh(geometry, mesh)) {
					continue;
				}
				const float       flip = Winding(mesh);
				const std::size_t begin = a_torso.points.size();
				if (Add(a_torso, geometry, mesh, flip) == 0) {
					continue;
				}
				++shapes;
				const bool skin = SkinShaded(geometry);
				for (std::size_t side = 0; side < kEyes; ++side) {
					if (auto tip = FindTip(geometry, mesh, flip, side)) {
						tip->begin = begin;
						tip->end = a_torso.points.size();
						tip->geometry = geometry;
						tip->skin = skin;
						tips[side].push_back(std::move(*tip));
					}
				}
				if (!skin) {
					covers.push_back({ geometry, NameOf(geometry), mesh.positions, mesh.triangles });
				}
			}
			if (a_torso.points.empty()) {
				a_torso.report = "no readable mesh on the torso";
				return;
			}

			std::array<std::pair<std::size_t, std::size_t>, kEyes> range{};
			for (std::size_t side = 0; side < kEyes; ++side) {
				// With several shapes over one breast (a body under a top), the one whose
				// tip is furthest out is the surface that shows. They compare in world
				// space: each tip is in its own tip bone's space, and two shapes can be
				// weighted to different breast bones (a 3BA top over a CBBE body).
				const auto&               candidates = tips[side];
				std::vector<RE::NiPoint3> at(candidates.size());
				RE::NiPoint3              outward;
				for (std::size_t i = 0; i < candidates.size(); ++i) {
					const auto& world = candidates[i].bone->world;
					at[i] = world * candidates[i].inBone;
					outward += world.rotate * candidates[i].axisInBone;
				}
				const Tip* chosen = nullptr;
				float      furthest = -std::numeric_limits<float>::max();
				if (outward.Unitize() > 1e-4f) {
					for (std::size_t i = 0; i < candidates.size(); ++i) {
						// ... among the shapes that cover the breast: a pendant or a pasty
						// weighted to the breast bone also sits out in front, on a sliver of
						// it. Vertex counts compare only between shapes on the same bone (an
						// outer breast bone carries a smaller part of the breast).
						std::size_t widest = 0;
						for (const auto& other : candidates) {
							if (other.boneName == candidates[i].boneName) {
								widest = std::max(widest, other.coverage);
							}
						}
						if (static_cast<float>(candidates[i].coverage) < kCoverShare * static_cast<float>(widest)) {
							continue;
						}
						const float out = at[i].Dot(outward);
						if (out > furthest) {
							furthest = out;
							chosen = &candidates[i];
						}
					}
				} else if (!tips[side].empty()) {
					chosen = &tips[side].front();
				}
				if (!chosen) {
					continue;
				}
				float nearest = std::numeric_limits<float>::max();
				for (const bool facing : { true, false }) {
					for (std::size_t k = chosen->begin; k < chosen->end; ++k) {
						const auto& point = a_torso.points[k];
						if (facing && point.normal.Dot(chosen->normal) < 0.2f) {
							continue;
						}
						const float distance = point.position.GetSquaredDistance(chosen->position);
						if (distance < nearest) {
							nearest = distance;
							a_torso.nipple[side] = k;
						}
					}
					if (a_torso.nipple[side] != kNone) {
						break;
					}
				}
				if (a_torso.nipple[side] != kNone) {
					a_torso.bone[side] = chosen->bone;
					range[side] = { chosen->begin, chosen->end };
					a_torso.how[side] = std::format("on {} by the weights of {} ({} vertices{})", chosen->shape, chosen->boneName, chosen->vertices,
						tips[side].size() > 1 ? std::format(", outermost of {} shapes", tips[side].size()) : std::string{});
				}
			}

			// No mesh weighted to a breast bone (a rigid cuirass): the surface nearest
			// the skeleton's own breast bone stands in.
			bool posed = false;
			for (std::size_t side = 0; side < kEyes; ++side) {
				if (a_torso.nipple[side] != kNone) {
					continue;
				}
				RE::NiAVObject* bone = nullptr;
				for (const char* name : side == kLeft ? std::array{ "Soft_LN00", "L Breast03", "L Breast02", "L Breast01", "NPC L Breast" } :
				                                        std::array{ "Soft_RN00", "R Breast03", "R Breast02", "R Breast01", "NPC R Breast" }) {
					bone = root->GetObjectByName(name);
					if (bone) {
						break;
					}
				}
				if (!bone) {
					a_torso.how[side] = "not found: no mesh is weighted to a breast bone and the skeleton has none";
					continue;
				}
				if (!posed) {
					WorldOf(a_torso);
					posed = true;
				}
				float nearest = kBoneReach * kBoneReach * bone->world.scale * bone->world.scale;
				float scale = 1.0f;
				for (std::size_t k = 0; k < a_torso.points.size(); ++k) {
					RE::NiPoint3 position;
					RE::NiPoint3 normal;
					SkinPoint(a_torso, a_torso.points[k], position, normal, scale);
					const float distance = position.GetSquaredDistance(bone->world.translate);
					if (distance < nearest) {
						nearest = distance;
						a_torso.nipple[side] = k;
					}
				}
				if (a_torso.nipple[side] == kNone) {
					a_torso.how[side] = std::format("not found: no surface near {}", NameOf(bone));
					continue;
				}
				a_torso.bone[side].reset(bone);
				range[side] = { 0, a_torso.points.size() };
				a_torso.how[side] = std::format("estimated: the surface nearest {} (no mesh is weighted to a breast bone)", NameOf(bone));
			}

			for (std::size_t side = 0; side < kEyes; ++side) {
				if (a_torso.nipple[side] == kNone) {
					continue;
				}
				const RE::NiPoint3 center = a_torso.points[a_torso.nipple[side]].position;
				for (std::size_t k = range[side].first; k < range[side].second; ++k) {
					if (a_torso.points[k].position.GetSquaredDistance(center) < kDuctReach * kDuctReach) {
						a_torso.aroundDucts.push_back(static_cast<std::uint32_t>(k));
					}
				}
			}
			std::ranges::sort(a_torso.aroundDucts);
			a_torso.aroundDucts.erase(std::ranges::unique(a_torso.aroundDucts).begin(), a_torso.aroundDucts.end());

			for (std::size_t side = 0; side < kEyes; ++side) {
				a_torso.cover[side] = CoverOf(tips[side], covers, a_actor);
			}
			const auto covered = [&](std::size_t a_side) {
				return a_torso.cover[a_side].empty() ? std::string{ ", bare" } : std::format(", covered by {}", a_torso.cover[a_side]);
			};
			a_torso.report = std::format("{} torso points on {} shape(s); left nipple {}{}; right nipple {}{}", a_torso.points.size(), shapes,
				a_torso.how[kLeft], covered(kLeft), a_torso.how[kRight], covered(kRight));
			a_torso.ok = true;
		}

		// A world-space box taken into each joint's own skin space, grown by a_slack
		// world units: a point lies in the box when its bind position, under its
		// heaviest joint, lies in that joint's box.
		void JointBoxes(const std::vector<RE::NiTransform>& a_world, const Region& a_region, float a_slack, std::vector<Box>& a_out)
		{
			a_out.resize(a_world.size());
			const std::array<RE::NiPoint3, 8> corners{
				RE::NiPoint3{ a_region.low.x, a_region.low.y, a_region.low.z },
				RE::NiPoint3{ a_region.high.x, a_region.low.y, a_region.low.z },
				RE::NiPoint3{ a_region.low.x, a_region.high.y, a_region.low.z },
				RE::NiPoint3{ a_region.high.x, a_region.high.y, a_region.low.z },
				RE::NiPoint3{ a_region.low.x, a_region.low.y, a_region.high.z },
				RE::NiPoint3{ a_region.high.x, a_region.low.y, a_region.high.z },
				RE::NiPoint3{ a_region.low.x, a_region.high.y, a_region.high.z },
				RE::NiPoint3{ a_region.high.x, a_region.high.y, a_region.high.z },
			};
			for (std::size_t j = 0; j < a_world.size(); ++j) {
				const RE::NiTransform inverse = a_world[j].Invert();
				Box                   box{ inverse * corners[0], inverse * corners[0] };
				for (std::size_t c = 1; c < corners.size(); ++c) {
					const RE::NiPoint3 p = inverse * corners[c];
					box.low = { std::min(box.low.x, p.x), std::min(box.low.y, p.y), std::min(box.low.z, p.z) };
					box.high = { std::max(box.high.x, p.x), std::max(box.high.y, p.y), std::max(box.high.z, p.z) };
				}
				const float slack = a_slack / std::max(a_world[j].scale, 1e-3f);
				box.low -= RE::NiPoint3{ slack, slack, slack };
				box.high += RE::NiPoint3{ slack, slack, slack };
				a_out[j] = box;
			}
		}

		bool Inside(const Box& a_box, const RE::NiPoint3& a_point)
		{
			return a_point.x >= a_box.low.x && a_point.y >= a_box.low.y && a_point.z >= a_box.low.z && a_point.x <= a_box.high.x &&
			       a_point.y <= a_box.high.y && a_point.z <= a_box.high.z;
		}

		void Pose(Torso& a_torso, const Region* a_region, const std::array<const Region*, kEyes>& a_watch)
		{
			auto& points = a_torso.points;
			WorldOf(a_torso);
			auto& world = a_torso.world;

			auto& candidates = a_torso.candidates;
			candidates.clear();
			auto& stamp = a_torso.stamp;
			if (stamp.size() != points.size()) {
				stamp.assign(points.size(), 0);
			}
			for (const auto k : a_torso.aroundDucts) {
				stamp[k] = frame;
				candidates.push_back(k);
			}
			if (a_region) {
				JointBoxes(world, *a_region, kRegionSlack, a_torso.boxes);
				for (std::size_t k = 0; k < points.size(); ++k) {
					if (stamp[k] == frame) {
						continue;
					}
					const auto& point = points[k];
					if (Inside(a_torso.boxes[point.joints[point.dominant]], point.position)) {
						candidates.push_back(static_cast<std::uint32_t>(k));
					}
				}
			}

			auto& patches = a_torso.patches;
			patches.resize(candidates.size());
			float scale = 1.0f;
			for (std::size_t i = 0; i < candidates.size(); ++i) {
				SkinPoint(a_torso, points[candidates[i]], patches[i].position, patches[i].normal, scale);
			}

			// Did the surface under a side's fluid move against that side's bone? Each
			// joint carries two probe points near the breast into the bone's space; a
			// joint whose probes moved since last frame is moving its skin there. The
			// side's own bone never does, which is the point of simulating in its frame.
			for (std::size_t side = 0; side < kEyes; ++side) {
				a_torso.moving[side] = false;
				auto* bone = a_torso.bone[side].get();
				auto& previous = a_torso.relative[side];
				if (!bone || a_torso.nipple[side] == kNone) {
					previous.clear();
					continue;
				}
				const RE::NiTransform toBone = bone->world.Invert();
				const RE::NiPoint3    probe = points[a_torso.nipple[side]].position;
				const RE::NiPoint3    below = probe - RE::NiPoint3{ 0.0f, 0.0f, 8.0f };
				const bool            compare = previous.size() == world.size();
				if (!compare) {
					previous.assign(world.size(), {});
				}
				auto& moved = a_torso.moved;
				moved.assign(world.size(), 0);
				bool any = false;
				for (std::size_t j = 0; j < world.size(); ++j) {
					const std::array<RE::NiPoint3, 2> now{ toBone * (world[j] * probe), toBone * (world[j] * below) };
					if (compare && (now[0].GetSquaredDistance(previous[j][0]) > kMoveStep * kMoveStep ||
									   now[1].GetSquaredDistance(previous[j][1]) > kMoveStep * kMoveStep)) {
						moved[j] = 1;
						any = true;
					}
					previous[j] = now;
				}
				if (!any || !a_watch[side]) {
					continue;
				}
				JointBoxes(world, *a_watch[side], kWatchSlack, a_torso.watchBoxes);
				for (const auto& point : points) {
					const auto joint = point.joints[point.dominant];
					if (moved[joint] && Inside(a_torso.watchBoxes[joint], point.position)) {
						a_torso.moving[side] = true;
						break;
					}
				}
			}

			const RE::NiPoint3 gravity{ 0.0f, 0.0f, -1.0f };
			for (std::size_t side = 0; side < kEyes; ++side) {
				auto& duct = a_torso.ducts[side][0];
				duct = {};
				const auto index = a_torso.nipple[side];
				if (index == kNone || index >= points.size()) {
					continue;
				}
				SkinPoint(a_torso, points[index], duct.point, duct.normal, scale);
				duct.down = Unit(gravity - duct.normal * gravity.Dot(duct.normal), gravity);
				duct.valid = true;
				a_torso.scale = scale;
			}
		}

		Torso* TorsoOf(RE::Actor* a_actor)
		{
			auto* root = a_actor ? a_actor->Get3D(false) : nullptr;
			if (!root || !root->parent) {
				return nullptr;
			}
			auto& torso = torsos[a_actor->GetHandle().native_handle()];
			torso.seen = clock;
			const bool fresh = torso.root.get() != root;
			if (!fresh && clock - torso.checked < kRecheck) {
				return &torso;
			}
			if (fresh) {
				torso = Torso{};
				torso.root.reset(root);
				torso.seen = clock;
			}
			torso.checked = clock;
			Signature signature = SignatureOf(root);
			if (fresh || signature != torso.signature || !torso.ok) {
				const std::string before = torso.report;
				torso.signature = std::move(signature);
				Build(torso, a_actor);
				torso.frame = 0;  // pose again this frame: the points are new
				// The first find always, a change only with the detailed log: a morph
				// re-applied on a timer rebuilds the torso every few seconds.
				if (torso.report != before && (before.empty() || Settings::Snapshot().debugLog)) {
					logger::info("{}: {}", ActorName(a_actor), torso.report);
				}
			}
			return &torso;
		}

		bool SameRegion(const Torso& a_torso, const Region* a_region)
		{
			if (!a_region) {
				return !a_torso.regionPosed;
			}
			return a_torso.regionPosed && a_torso.region.low == a_region->low && a_torso.region.high == a_region->high;
		}
	}

	void BeginFrame(float a_delta)
	{
		++frame;
		clock += a_delta;
		std::erase_if(torsos, [](const auto& a_entry) {
			const auto& torso = a_entry.second;
			return clock - torso.seen > kForget || !torso.root || !torso.root->parent;
		});
	}

	std::optional<View> Of(RE::Actor* a_actor, const Placement&, const Region* a_region, const std::array<const Region*, kEyes>& a_watch)
	{
		auto* torso = TorsoOf(a_actor);
		if (!torso || !torso->ok) {
			return std::nullopt;
		}
		if (torso->frame != frame || !SameRegion(*torso, a_region)) {
			// A second pose in one frame (a wider region after fluid woke) sees no new
			// motion, since the first already took the joints' places; keep its verdict.
			const bool again = torso->frame == frame;
			const auto moving = torso->moving;
			torso->frame = frame;
			torso->regionPosed = a_region != nullptr;
			if (a_region) {
				torso->region = *a_region;
			}
			Pose(*torso, a_region, a_watch);
			for (std::size_t side = 0; again && side < kEyes; ++side) {
				torso->moving[side] = torso->moving[side] || moving[side];
			}
		}
		View view{ torso->patches, torso->ducts, torso->scale, torso->moving[kLeft] || torso->moving[kRight], {}, {}, torso->moving };
		for (std::size_t side = 0; side < kEyes; ++side) {
			view.bones[side] = torso->bone[side].get();
		}
		return view;
	}

	bool Nipple(RE::Actor* a_actor, std::size_t a_side, RE::NiPoint3& a_point, RE::NiPoint3& a_normal, RE::NiAVObject** a_bone)
	{
		auto* torso = TorsoOf(a_actor);
		if (!torso || !torso->ok || a_side >= kEyes) {
			return false;
		}
		if (torso->frame != frame) {
			torso->frame = frame;
			torso->regionPosed = false;
			Pose(*torso, nullptr, {});
		}
		const auto& duct = torso->ducts[a_side][0];
		if (!duct.valid) {
			return false;
		}
		a_point = duct.point;
		a_normal = duct.normal;
		if (a_bone) {
			*a_bone = torso->bone[a_side].get();
		}
		return true;
	}

	bool Covered(RE::Actor* a_actor, std::size_t a_side)
	{
		auto* torso = TorsoOf(a_actor);
		return torso && torso->ok && a_side < kEyes && !torso->cover[a_side].empty();
	}

	std::array<RE::NiAVObject*, kEyes> Bones(RE::Actor* a_actor)
	{
		std::array<RE::NiAVObject*, kEyes> bones{};
		if (auto* torso = TorsoOf(a_actor); torso && torso->ok) {
			for (std::size_t side = 0; side < kEyes; ++side) {
				bones[side] = torso->bone[side].get();
			}
		}
		return bones;
	}

	std::string Describe(RE::Actor* a_actor, const Placement&)
	{
		auto* torso = TorsoOf(a_actor);
		if (!torso) {
			return "no 3D loaded";
		}
		return torso->report;
	}

	std::string ActorName(RE::Actor* a_actor)
	{
		const char* name = a_actor ? a_actor->GetDisplayFullName() : nullptr;
		return name && *name ? name : "someone";
	}

	void Reset()
	{
		torsos.clear();
	}
}
