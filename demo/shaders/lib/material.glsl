struct MaterialInfo {
	vec3 baseColor;
	
	// specular color at angles 0(direct) and 90(grazing) 
	vec3 specularColor0;
	vec3 specularColor90;
	
	float perceptualRoughness; 
	float metallic;
    
    float ior;
    
    vec3 diffuseColor;
	
	float specularWeight;
};
