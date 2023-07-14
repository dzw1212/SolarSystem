#pragma once

struct PointLight
{
	//衰减
	float K_constant; //常数项
	float K_linear;		//一次向 
	float K_quadratic;

};