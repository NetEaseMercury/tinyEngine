struct PickPush {
    float4x4 model;
    uint     objectId;
};
[[vk::push_constant]] PickPush push;

uint main() : SV_TARGET {
    return push.objectId;
}
