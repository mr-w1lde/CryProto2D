#pragma once

struct FFlipbookAnim
{
    string name;
    int startFrame;
    int endFrame;
    int row;
    float fps;
    bool loop;

    bool operator ==(const FFlipbookAnim& other) const
    {
       return name == other.name;
    }
};
