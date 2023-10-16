#pragma once

#include <string>
#include <vector>
#include <directxmath.h>
#include <Windows.h>
#include <algorithm>
using namespace DirectX;
using namespace std;


enum class MeshType
{
	CUBE,
	SPHERE,
	CYLINDER,
	CONE,
	QUAD
};


struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 TexCoord;
	XMFLOAT3 Tangent;
	XMFLOAT3 BiTangent;
};


struct InstanceData
{
	XMFLOAT3 Pos;

	bool operator==(const InstanceData& a) const
	{
		return (fabs(a.Pos.x - Pos.x) < FLT_EPSILON && fabs(a.Pos.y - Pos.y) < FLT_EPSILON && fabs(a.Pos.z - Pos.z) < FLT_EPSILON);
	}
};

class VerticeShapes
{
private:
	vector<SimpleVertex> vertices;
	vector<WORD> indices;
	vector<InstanceData> mInstanceData;
	XMFLOAT4X4 transform;
	XMFLOAT3 translation;
	XMFLOAT3 rotation;
	XMFLOAT3 scale;
	XMFLOAT3 wolrdPos;
	wstring diffuseTexture;
	wstring normalMap;
	wstring displacementMap;
	wstring shader;
	string name;
	MeshType geometryType;
	
	bool particlesSystem;

	void Cube();
	void Sphere() const;
	void Cylinder();
	void Cone();
	void Quad();

public:
	VerticeShapes();
	VerticeShapes(const MeshType& geometryType, const XMFLOAT4X4& entityTransform, const XMFLOAT3& translation, const XMFLOAT3& rotation,
		const XMFLOAT3& scale, const wstring& diffuseTexture, const wstring& normalMap, const wstring& displacementMap, const wstring& shader,
		const string& name, const vector<InstanceData>* const instanceData, const bool& particleSystem);
	~VerticeShapes();

	const vector<SimpleVertex>& Vertices() const;
	const vector<WORD>& Indices() const;
	const vector<InstanceData>& Instances() const;
	const wstring& DiffuseTexture() const;
	const wstring& NormalMap() const;
	const wstring& DisplacementMap() const;
	const wstring& Shader() const;
	const XMFLOAT3& WorldPos() const;
	const XMFLOAT4X4& Transform() const;
	const MeshType& Geometry() const;
	const string& Name() const;
	const bool& ParticleSystem() const;

	void SetTransform(const XMFLOAT4X4& entityTransform);
	void RemoveInstances(const vector<InstanceData>& instancesToRemove);
};