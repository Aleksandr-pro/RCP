#pragma once

#include "SurfaceBounce.h"

namespace atlas::physics
{
    enum class BodyMaterial
    {
        TestRubber,
        TestWood
    };

    enum class SurfaceMaterial
    {
        Stone,
        Ground,
        Sand
    };

    // Возвращает настройки для конкретной пары материалов.
    // Значения экспериментальные, не физическая справочная таблица.
    inline bool getContactMaterial(
        BodyMaterial body,
        SurfaceMaterial surface,
        ContactMaterial& result
    )
    {
        float restitution = 0.0f;

        switch (body)
        {
            case BodyMaterial::TestRubber:
                switch (surface)
                {
                    case SurfaceMaterial::Stone:
                        restitution = 0.70f;
                        break;

                    case SurfaceMaterial::Ground:
                        restitution = 0.40f;
                        break;

                    case SurfaceMaterial::Sand:
                        restitution = 0.10f;
                        break;

                    default:
                        return false;
                }
                break;

            case BodyMaterial::TestWood:
                switch (surface)
                {
                    case SurfaceMaterial::Stone:
                        restitution = 0.35f;
                        break;

                    case SurfaceMaterial::Ground:
                        restitution = 0.20f;
                        break;

                    case SurfaceMaterial::Sand:
                        restitution = 0.05f;
                        break;

                    default:
                        return false;
                }
                break;

            default:
                return false;
        }

        result.restitution = restitution;
        result.restSpeed = 0.20f;
        return true;
    }
    // Временные тестовые коэффициенты скольжения.
    // Это настройки прототипа, а не измеренные свойства материалов.
    inline bool getSlidingFriction(
        BodyMaterial body,
        SurfaceMaterial surface,
        float& result
    )
    {
        float friction = 0.0f;

        switch (body)
        {
            case BodyMaterial::TestRubber:
                switch (surface)
                {
                    case SurfaceMaterial::Stone:
                        friction = 0.60f;
                        break;
                    case SurfaceMaterial::Ground:
                        friction = 0.80f;
                        break;
                    case SurfaceMaterial::Sand:
                        friction = 1.00f;
                        break;
                    default:
                        return false;
                }
                break;

            case BodyMaterial::TestWood:
                switch (surface)
                {
                    case SurfaceMaterial::Stone:
                        friction = 0.35f;
                        break;
                    case SurfaceMaterial::Ground:
                        friction = 0.55f;
                        break;
                    case SurfaceMaterial::Sand:
                        friction = 0.80f;
                        break;
                    default:
                        return false;
                }
                break;

            default:
                return false;
        }

        result = friction;
        return true;
    }
}
