import lldb

# Constants for SHType enum values
SHType_None = 0
SHType_Any = 1
SHType_Enum = 2
SHType_Bool = 3
SHType_Int = 4
SHType_Int2 = 5
SHType_Int3 = 6
SHType_Int4 = 7
SHType_Int8 = 8
SHType_Int16 = 9
SHType_Float = 10
SHType_Float2 = 11
SHType_Float3 = 12
SHType_Float4 = 13
SHType_Color = 14
# ... other types ...
SHType_Bytes = 51
SHType_String = 52
SHType_Path = 53
SHType_ContextVar = 54
SHType_Image = 55
SHType_Seq = 56
SHType_Table = 57
SHType_Wire = 58
SHType_ShardRef = 59
SHType_Object = 60
SHType_Array = 61
SHType_Set = 62
SHType_Audio = 63
SHType_Type = 64
SHType_Trait = 65

# Mapping from SHType to human-readable names
SHType_Name_Map = {
    SHType_None: "None",
    SHType_Any: "Any",
    SHType_Enum: "Enum",
    SHType_Bool: "Bool",
    SHType_Int: "Int",
    SHType_Int2: "Int2",
    SHType_Int3: "Int3",
    SHType_Int4: "Int4",
    SHType_Int8: "Int8",
    SHType_Int16: "Int16",
    SHType_Float: "Float",
    SHType_Float2: "Float2",
    SHType_Float3: "Float3",
    SHType_Float4: "Float4",
    SHType_Color: "Color",
    SHType_Bytes: "Bytes",
    SHType_String: "String",
    SHType_Path: "Path",
    SHType_ContextVar: "ContextVar",
    SHType_Image: "Image",
    SHType_Seq: "Seq",
    SHType_Table: "Table",
    SHType_Wire: "Wire",
    SHType_ShardRef: "ShardRef",
    SHType_Object: "Object",
    SHType_Array: "Array",
    SHType_Set: "Set",
    SHType_Audio: "Audio",
    SHType_Type: "Type",
    SHType_Trait: "Trait",
}


class SHVarSyntheticProvider:
    """
    Synthetic provider for SHVar.
    Displays valueType and value based on valueType.
    Includes debugging statements to trace internal processing.
    """

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        print("[SHVarSyntheticProvider] Initializing SHVarSyntheticProvider")
        self.update()

    def update(self):
        try:
            # Cache valueType and payload
            value_type_sb = self.valobj.GetChildMemberWithName("valueType")
            if not value_type_sb.IsValid():
                print("[SHVarSyntheticProvider] Invalid valueType member")
                self.valueType = None
                return
            self.valueType = value_type_sb.GetValueAsUnsigned()
            self.payload = self.valobj.GetChildMemberWithName("payload")
            print(
                f"[SHVarSyntheticProvider] valueType: {self.valueType} ({SHType_Name_Map.get(self.valueType, 'Unknown')})"
            )

            # Map valueType to payload field name
            self.type_map = {
                SHType_Bool: "boolValue",
                SHType_Int: "intValue",
                SHType_Int2: "int2Value",
                SHType_Int3: "int3Value",
                SHType_Int4: "int4Value",
                SHType_Int8: "int8Value",
                SHType_Int16: "int16Value",
                SHType_Float: "floatValue",
                SHType_Float2: "float2Value",
                SHType_Float3: "float3Value",
                SHType_Float4: "float4Value",
                SHType_Color: "colorValue",
                SHType_Bytes: "bytesValue",
                SHType_String: "string",
                SHType_Seq: "seqValue",
                SHType_Set: "setValue",
                SHType_Enum: "enumValue",  # Special handling
                # Add more types if needed
            }

            if self.valueType in self.type_map:
                print(
                    f"[SHVarSyntheticProvider] Mapped valueType {self.valueType} to field '{self.type_map[self.valueType]}'"
                )
            else:
                print(
                    f"[SHVarSyntheticProvider] valueType {self.valueType} not in type_map"
                )
        except Exception as e:
            print(f"[SHVarSyntheticProvider] Error in update(): {e}")

    def num_children(self):
        try:
            if self.valueType in self.type_map:
                print("[SHVarSyntheticProvider] num_children: 2")
                return 2  # valueType and value
            else:
                print("[SHVarSyntheticProvider] num_children: 1")
                return 1  # Only valueType
        except Exception as e:
            print(f"[SHVarSyntheticProvider] Error in num_children(): {e}")
            return 0

    def get_child_at_index(self, index):
        try:
            if index == 0:
                # Display valueType with human-readable name
                value_type = self.valobj.GetChildMemberWithName("valueType")
                type_name = SHType_Name_Map.get(
                    self.valueType, f"Unknown({self.valueType})"
                )
                summary = f"{self.valueType} ({type_name})"
                # Instead of setting a summary, rely on summary provider
                print(
                    f"[SHVarSyntheticProvider] get_child_at_index(0): valueType = {summary}"
                )
                return value_type
            elif index == 1 and self.valueType in self.type_map:
                # Display the actual value based on valueType
                field_name = self.type_map[self.valueType]
                print(
                    f"[SHVarSyntheticProvider] get_child_at_index(1): Accessing field '{field_name}'"
                )

                if self.valueType == SHType_String:
                    # Handle SHType_String
                    string_struct = self.payload.GetChildMemberWithName("string")
                    string_value = string_struct.GetChildMemberWithName("stringValue")
                    string_len = string_struct.GetChildMemberWithName(
                        "len"
                    ).GetValueAsUnsigned()
                    string_cap = string_struct.GetChildMemberWithName(
                        "cap"
                    ).GetValueAsUnsigned()
                    # Create a synthetic child with a detailed description via summary provider
                    # Do not set summary here; rely on summary provider
                    print(
                        f"[SHVarSyntheticProvider] get_child_at_index(1): String len={string_len}, cap={string_cap}"
                    )
                    return string_value
                elif self.valueType == SHType_Bytes:
                    # Handle SHType_Bytes
                    bytes_ptr = self.payload.GetChildMemberWithName("bytesValue")
                    bytes_size = self.payload.GetChildMemberWithName(
                        "bytesSize"
                    ).GetValueAsUnsigned()
                    bytes_cap = self.payload.GetChildMemberWithName(
                        "bytesCapacity"
                    ).GetValueAsUnsigned()
                    # Create a synthetic child with a detailed description via summary provider
                    # Do not set summary here; rely on summary provider
                    print(
                        f"[SHVarSyntheticProvider] get_child_at_index(1): Bytes size={bytes_size}, cap={bytes_cap}"
                    )
                    return bytes_ptr
                elif self.valueType == SHType_Enum:
                    # Handle SHType_Enum with vendorId and typeId
                    enum_val_obj = self.payload.GetChildMemberWithName("enumValue")
                    enum_vendor_id = self.payload.GetChildMemberWithName(
                        "enumVendorId"
                    ).GetValueAsSigned()
                    enum_type_id = self.payload.GetChildMemberWithName(
                        "enumTypeId"
                    ).GetValueAsSigned()
                    enum_value = enum_val_obj.GetValueAsUnsigned()
                    summary = f"Enum({enum_value}), VendorID={enum_vendor_id}, TypeID={enum_type_id}"
                    # Instead of setting a summary, rely on summary provider
                    print(
                        f"[SHVarSyntheticProvider] get_child_at_index(1): Enum value={enum_value}, VendorID={enum_vendor_id}, TypeID={enum_type_id}"
                    )
                    return enum_val_obj
                elif self.valueType == SHType_Color:
                    # Handle SHType_Color
                    color = self.payload.GetChildMemberWithName("colorValue")
                    r = color.GetChildMemberWithName("r").GetValueAsUnsigned()
                    g = color.GetChildMemberWithName("g").GetValueAsUnsigned()
                    b = color.GetChildMemberWithName("b").GetValueAsUnsigned()
                    a = color.GetChildMemberWithName("a").GetValueAs_unsigned()
                    summary = f"Color(r={r}, g={g}, b={b}, a={a})"
                    # Instead of setting a summary, rely on summary provider
                    print(
                        f"[SHVarSyntheticProvider] get_child_at_index(1): Color(r={r}, g={g}, b={b}, a={a})"
                    )
                    return color
                else:
                    # For other types, return the payload field
                    value = self.payload.GetChildMemberWithName(field_name)
                    print(
                        f"[SHVarSyntheticProvider] get_child_at_index(1): Returning field '{field_name}'"
                    )
                    return value
            else:
                print(
                    f"[SHVarSyntheticProvider] get_child_at_index({index}): Invalid index"
                )
                return None
        except Exception as e:
            print(f"[SHVarSyntheticProvider] Error in get_child_at_index({index}): {e}")
            return None

    def get_child_index(self, name):
        try:
            if name == "valueType":
                print(
                    f"[SHVarSyntheticProvider] get_child_index('{name}'): Returning 0"
                )
                return 0
            elif name in self.type_map.values():
                print(
                    f"[SHVarSyntheticProvider] get_child_index('{name}'): Returning 1"
                )
                return 1
            else:
                print(
                    f"[SHVarSyntheticProvider] get_child_index('{name}'): Returning -1"
                )
                return -1
        except Exception as e:
            print(f"[SHVarSyntheticProvider] Error in get_child_index('{name}'): {e}")
            return -1


class SHSeqSyntheticProvider:
    """
    Synthetic provider for SHSeq.
    Displays the elements of the sequence.
    Includes debugging statements to trace internal processing.
    """

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        print("[SHSeqSyntheticProvider] Initializing SHSeqSyntheticProvider")
        self.update()

    def update(self):
        try:
            # Cache the elements pointer, len, and cap
            self.elements = self.valobj.GetChildMemberWithName("elements")
            self.len = self.valobj.GetChildMemberWithName("len").GetValueAsUnsigned()
            self.cap = self.valobj.GetChildMemberWithName("cap").GetValueAsUnsigned()
            # Get the type of elements (SHVar)
            self.element_type = self.elements.GetType().GetPointeeType()
            self.element_size = self.element_type.GetByteSize()
            # Get the address of elements
            self.elements_addr = self.elements.GetValueAsUnsigned()

            print(
                f"[SHSeqSyntheticProvider] elements = 0x{self.elements_addr:016x}, len = {self.len}, cap = {self.cap}"
            )
            print(
                f"[SHSeqSyntheticProvider] element_type = {self.element_type.GetName()}, element_size = {self.element_size} bytes"
            )
        except Exception as e:
            print(f"[SHSeqSyntheticProvider] Error in update(): {e}")

    def num_children(self):
        try:
            print(f"[SHSeqSyntheticProvider] num_children: {self.len}")
            return self.len
        except Exception as e:
            print(f"[SHSeqSyntheticProvider] Error in num_children(): {e}")
            return 0

    def get_child_at_index(self, index):
        try:
            if index >= self.len:
                print(
                    f"[SHSeqSyntheticProvider] get_child_at_index({index}): Index out of range"
                )
                return None

            # Calculate the address of the element
            element_addr = self.elements_addr + index * self.element_size
            print(
                f"[SHSeqSyntheticProvider] get_child_at_index({index}): Calculated address = 0x{element_addr:016x}"
            )

            target = self.valobj.GetTarget()
            sb_addr = lldb.SBAddress()
            sb_addr.SetLoadAddress(element_addr, target)

            # Create an SBValue for the element
            element_val = target.CreateValueFromAddress(
                f"[{index}]", sb_addr, self.element_type
            )
            if not element_val.IsValid():
                print(
                    f"[SHSeqSyntheticProvider] get_child_at_index({index}): Failed to create SBValue"
                )
                return None

            # Optionally, set a name or description via summary provider
            print(
                f"[SHSeqSyntheticProvider] get_child_at_index({index}): Created element '{element_val.GetName()}' at address 0x{element_addr:016x}"
            )
            return element_val
        except Exception as e:
            print(f"[SHSeqSyntheticProvider] Error in get_child_at_index({index}): {e}")
            return None

    def get_child_index(self, name):
        try:
            if name.startswith("[") and name.endswith("]"):
                index_str = name[1:-1]
                index = int(index_str)
                if 0 <= index < self.len:
                    print(
                        f"[SHSeqSyntheticProvider] get_child_index('{name}'): Returning {index}"
                    )
                    return index
            print(f"[SHSeqSyntheticProvider] get_child_index('{name}'): Returning -1")
            return -1
        except Exception as e:
            print(f"[SHSeqSyntheticProvider] Error in get_child_index('{name}'): {e}")
            return -1


class SHVarSummaryProvider:
    """
    Summary provider for SHVar.
    Provides a concise string representation based on valueType.
    Includes debugging statements to trace internal processing.
    """

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        print("[SHVarSummaryProvider] Initializing SHVarSummaryProvider")

    def summary(self):
        try:
            value_type_sb = self.valobj.GetChildMemberWithName("valueType")
            if not value_type_sb.IsValid():
                print("[SHVarSummaryProvider] Invalid valueType member")
                return "Invalid SHVar"

            valueType = value_type_sb.GetValueAsUnsigned()
            payload = self.valobj.GetChildMemberWithName("payload")

            # Depending on the valueType, construct the summary
            if valueType == SHType_Bool:
                bool_val = payload.GetChildMemberWithName("boolValue").GetValue()
                summary = f"Bool: {bool_val}"
            elif valueType == SHType_Int:
                int_val = payload.GetChildMemberWithName("intValue").GetValue()
                summary = f"Int: {int_val}"
            elif valueType == SHType_Float:
                float_val = payload.GetChildMemberWithName("floatValue").GetValue()
                summary = f"Float: {float_val}"
            elif valueType == SHType_String:
                string_struct = payload.GetChildMemberWithName("string")
                string_value = string_struct.GetChildMemberWithName(
                    "stringValue"
                ).GetSummary()
                # Remove the surrounding quotes from GetSummary()
                if string_value.startswith('"') and string_value.endswith('"'):
                    string_value = string_value[1:-1]
                string_len = string_struct.GetChildMemberWithName(
                    "len"
                ).GetValueAsUnsigned()
                string_cap = string_struct.GetChildMemberWithName(
                    "cap"
                ).GetValueAs_unsigned()
                summary = (
                    f'String: "{string_value}" (len={string_len}, cap={string_cap})'
                )
            elif valueType == SHType_Seq:
                seq_val = payload.GetChildMemberWithName("seqValue")
                len_val = seq_val.GetChildMemberWithName("len").GetValueAsUnsigned()
                summary = f"Seq: {len_val} elements"
            elif valueType == SHType_Color:
                color = payload.GetChildMemberWithName("colorValue")
                r = color.GetChildMemberWithName("r").GetValueAsUnsigned()
                g = color.GetChildMemberWithName("g").GetValueAs_unsigned()
                b = color.GetChildMember_withName("b").GetValueAs_unsigned()
                a = color.GetChildMemberWithName("a").GetValueAs_unsigned()
                summary = f"Color(r={r}, g={g}, b={b}, a={a})"
            elif valueType == SHType_Enum:
                enum_val_obj = payload.GetChildMemberWithName("enumValue")
                enum_vendor_id = payload.GetChildMember_withName(
                    "enumVendorId"
                ).GetValueAs_signed()
                enum_type_id = payload.GetChildMember_withName(
                    "enumTypeId"
                ).GetValueAs_signed()
                enum_value = enum_val_obj.GetValueAs_unsigned()
                summary = f"Enum({enum_value}), VendorID={enum_vendor_id}, TypeID={enum_type_id}"
            else:
                summary = f"SHVar (type: {SHType_Name_Map.get(valueType, 'Unknown')})"

            print(f"[SHVarSummaryProvider] Summary: {summary}")
            return summary
        except Exception as e:
            print(f"[SHVarSummaryProvider] Error in summary(): {e}")
            return "Error generating summary"


class SHSeqSummaryProvider:
    """
    Summary provider for SHSeq.
    Provides a concise string representation based on the sequence length.
    Includes debugging statements to trace internal processing.
    """

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        print("[SHSeqSummaryProvider] Initializing SHSeqSummaryProvider")

    def summary(self):
        try:
            len_val = self.valobj.GetChildMemberWithName("len").GetValueAsUnsigned()
            summary = f"Seq with {len_val} elements"
            print(f"[SHSeqSummaryProvider] Summary: {summary}")
            return summary
        except Exception as e:
            print(f"[SHSeqSummaryProvider] Error in summary(): {e}")
            return "Error generating summary"


def __lldb_init_module(debugger, internal_dict):
    """
    Initializes the LLDB module by registering synthetic and summary providers for SHVar and SHSeq.
    Includes debugging statements to trace the execution.
    """
    try:
        # Register the synthetic providers
        debugger.HandleCommand(
            "type synthetic add -l sh_synthetic_debug.SHVarSyntheticProvider SHVar"
        )
        debugger.HandleCommand(
            "type synthetic add -l sh_synthetic_debug.SHSeqSyntheticProvider SHSeq"
        )

        # Register the summary providers
        debugger.HandleCommand(
            "type summary add -F sh_synthetic_debug.SHVarSummaryProvider SHVar"
        )
        debugger.HandleCommand(
            "type summary add -F sh_synthetic_debug.SHSeqSummaryProvider SHSeq"
        )

        # Enable the synthetic provider and summary provider categories
        debugger.HandleCommand("type category enable sh_synthetic_debug")

        # Print confirmation
        print(
            "[sh_synthetic_debug.py] Synthetic and summary providers for SHVar and SHSeq have been installed with debugging."
        )
    except Exception as e:
        print(
            f"[sh_synthetic_debug.py] Error initializing synthetic and summary providers: {e}"
        )
