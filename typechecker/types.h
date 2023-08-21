#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <unordered_map>


#ifndef __TYPES_H__
#define __TYPES_H__


namespace Typechecker
{
    enum TYPE_NAME {INT, FLOAT, BOOL, ARRAY, TUPLE};

    std::string typenameToString(TYPE_NAME type_name)
    {
        switch (type_name)
        {
        case INT:
            return "IntType";
        case FLOAT:
            return "FloatType";
        case BOOL:
            return "BoolType";
        case ARRAY:
            return "ArrayType";
        case TUPLE:
            return "TupleType";
        }
    }

    class ResolvedType
    {
        public:
            virtual std::string toString() {return typenameToString(type_name);};
            const TYPE_NAME type_name;
            virtual bool operator== (const ResolvedType& other)
            {
                return type_name == other.type_name;
            }
            
            bool operator!= (const ResolvedType& other)
            {
                // NOTE: ensure INHERITED operator== is called here. Not base operator==.
                return !(operator==(other));
            }
            
            virtual ~ResolvedType() {}
            virtual ResolvedType* clone() = 0;

        protected:
            ResolvedType(TYPE_NAME _tn) : type_name(_tn) {}
    };

    class IntRType : public ResolvedType
    {
        public:
            IntRType() : ResolvedType(INT) {}
            virtual ResolvedType* clone()
            {
                return (ResolvedType*) new IntRType();
            }
    };

    class FloatRType : public ResolvedType
    {
        public:
            FloatRType() : ResolvedType(FLOAT) {}
            virtual ResolvedType* clone()
            {
                return (ResolvedType*) new FloatRType();
            }
    };

    class BoolRType : public ResolvedType
    {
        public:
            BoolRType() : ResolvedType(BOOL) {}
            virtual ResolvedType* clone()
            {
                return (ResolvedType*) new BoolRType();
            }
    };

    class ArrayRType : public ResolvedType
    {
        public:
            std::shared_ptr<ResolvedType> element_type;
            int rank;
            ArrayRType(std::shared_ptr<ResolvedType>& _et, const int& _r) : ResolvedType(ARRAY), element_type(_et), rank(_r) {}
            virtual bool operator==(const ResolvedType& other)
            {
                if (other.type_name != ARRAY)
                    return false;
                
                const ArrayRType& other_array = static_cast<const ArrayRType&>(other);

                bool sameRank = rank == other_array.rank;
                bool sameElementType = *element_type.get() == *other_array.element_type.get();  

                return sameRank && sameElementType;
            }

            virtual std::string toString()
            {
                if (element_type.get() == nullptr)
                    std::printf("Got it\n");
                return typenameToString(type_name) + " (" + element_type.get()->toString() + ") " + std::to_string(rank);
            }
            
            virtual ResolvedType* clone()
            {
                return (ResolvedType*) new ArrayRType(element_type, rank);
            }

            static std::shared_ptr<ArrayRType> make_array(std::shared_ptr<ResolvedType>& elm_type, const int& rank)
            {
                return std::make_shared<ArrayRType>(elm_type, rank);
            }
    };

    class TupleRType : public ResolvedType
    {
        public:
            std::vector<std::shared_ptr<ResolvedType>> element_types;
            TupleRType(const std::vector<std::shared_ptr<ResolvedType>>& _ets) : ResolvedType(TUPLE)
            {
                for(const std::shared_ptr<ResolvedType>&  element_type : _ets)
                {
                    element_types.push_back(element_type);
                }
            }

            virtual bool operator==(const ResolvedType& other)
            {
                if (other.type_name != TUPLE)
                    return false;
                
                const TupleRType& other_tuple = static_cast<const TupleRType&>(other);

                if (element_types.size() != other_tuple.element_types.size())
                    return false;
                
                for(int i = 0; i < element_types.size(); i++)
                {
                    ResolvedType* first = element_types[i].get();
                    ResolvedType* second = other_tuple.element_types[i].get();

                    if (*first != *second)
                        return false;
                }

                return true;
            }

            virtual std::string toString()
            {
                std::string types_str = "";

                for(const std::shared_ptr<ResolvedType>& element_type : element_types)
                {
                    types_str += " (" + element_type.get()->toString() + ")";
                }

                return typenameToString(type_name) + types_str;
            }
            
            virtual ResolvedType* clone()
            {
                return (ResolvedType*) new TupleRType(element_types);
            }
    };
} // namespace Typechecker

#endif